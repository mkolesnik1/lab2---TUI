#include "infrastructure/sqlite_index.h"

#include <sqlite3.h>

#include <filesystem>
#include <sstream>

#include "core/domain/errors.h"

namespace mail {

void SqliteIndex::Closer::operator()(sqlite3* db) const noexcept { sqlite3_close_v2(db); }

namespace {

struct StmtFinalizer {
  void operator()(sqlite3_stmt* s) const noexcept { sqlite3_finalize(s); }
};

[[noreturn]] void fail(sqlite3* db, const std::string& what) {
  throw StoreError("SQLite: " + what + ": " + sqlite3_errmsg(db));
}

// RAII-обёртка над подготовленным запросом.
class Stmt {
 public:
  Stmt(sqlite3* db, const char* sql) : db_(db) {
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) fail(db, "prepare");
    st_.reset(raw);
  }
  Stmt& bind(int i, const std::string& v) {
    if (sqlite3_bind_text(st_.get(), i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT) != SQLITE_OK)
      fail(db_, "bind");
    return *this;
  }
  Stmt& bind(int i, long long v) {
    if (sqlite3_bind_int64(st_.get(), i, v) != SQLITE_OK) fail(db_, "bind");
    return *this;
  }
  bool step() {
    const int rc = sqlite3_step(st_.get());
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    fail(db_, "step");
  }
  void run() {
    while (step()) {
    }
  }
  std::string text(int col) const {
    const unsigned char* t = sqlite3_column_text(st_.get(), col);
    return t ? std::string(reinterpret_cast<const char*>(t), static_cast<size_t>(sqlite3_column_bytes(st_.get(), col)))
             : std::string();
  }
  long long integer(int col) const { return sqlite3_column_int64(st_.get(), col); }
  void reset() { sqlite3_reset(st_.get()); }

 private:
  sqlite3* db_;
  std::unique_ptr<sqlite3_stmt, StmtFinalizer> st_;
};

void exec(sqlite3* db, const char* sql) {
  if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) fail(db, sql);
}

// Транзакция: если commit() не вызван (исключение) — откат, индекс остаётся согласованным.
class Transaction {
 public:
  explicit Transaction(sqlite3* db) : db_(db) { exec(db_, "BEGIN IMMEDIATE"); }
  ~Transaction() {
    if (!done_) sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
  }
  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;
  void commit() {
    exec(db_, "COMMIT");
    done_ = true;
  }

 private:
  sqlite3* db_;
  bool done_ = false;
};

std::string joinRefs(const std::vector<std::string>& refs) {
  std::string s;
  for (const auto& r : refs) {
    if (!s.empty()) s += ' ';
    s += r;
  }
  return s;
}

std::vector<std::string> splitRefs(const std::string& s) {
  std::vector<std::string> out;
  std::istringstream in(s);
  for (std::string r; in >> r;) out.push_back(r);
  return out;
}

}  // namespace

SqliteIndex::SqliteIndex(const std::string& dbPath) {
  sqlite3* raw = nullptr;
  if (sqlite3_open(dbPath.c_str(), &raw) != SQLITE_OK) {
    const std::string msg = raw ? sqlite3_errmsg(raw) : "нет памяти";
    sqlite3_close_v2(raw);
    throw StoreError("не удалось открыть индекс " + dbPath + ": " + msg);
  }
  db_.reset(raw);
  sqlite3_busy_timeout(raw, 3000);
  exec(raw, "PRAGMA journal_mode=WAL");
  exec(raw,
       "CREATE TABLE IF NOT EXISTS messages("
       " id TEXT PRIMARY KEY, message_id TEXT NOT NULL, in_reply_to TEXT NOT NULL, refs TEXT NOT NULL,"
       " sender TEXT NOT NULL, subject TEXT NOT NULL, date INTEGER NOT NULL, flags TEXT NOT NULL,"
       " added_at INTEGER NOT NULL)");
  exec(raw,
       "CREATE TABLE IF NOT EXISTS sync_log("
       " id INTEGER PRIMARY KEY AUTOINCREMENT, at INTEGER NOT NULL, added INTEGER NOT NULL,"
       " removed INTEGER NOT NULL, total INTEGER NOT NULL)");
}

SqliteIndex::~SqliteIndex() = default;

std::vector<MessageSummary> SqliteIndex::loadAll() const {
  std::lock_guard lock(mutex_);
  Stmt st(db_.get(), "SELECT id, message_id, in_reply_to, refs, sender, subject, date, flags FROM messages ORDER BY date, id");
  std::vector<MessageSummary> out;
  while (st.step()) {
    MessageSummary m;
    m.id = st.text(0);
    m.messageId = st.text(1);
    m.inReplyTo = st.text(2);
    m.references = splitRefs(st.text(3));
    m.from = st.text(4);
    m.subject = st.text(5);
    m.date = static_cast<std::time_t>(st.integer(6));
    m.flags = Flags::fromMaildir(st.text(7));
    out.push_back(std::move(m));
  }
  return out;
}

std::unordered_map<StoreId, Flags> SqliteIndex::knownFlags() const {
  std::lock_guard lock(mutex_);
  Stmt st(db_.get(), "SELECT id, flags FROM messages");
  std::unordered_map<StoreId, Flags> out;
  while (st.step()) out.emplace(st.text(0), Flags::fromMaildir(st.text(1)));
  return out;
}

void SqliteIndex::apply(const IndexChanges& changes, const SyncLogEntry& log) {
  std::lock_guard lock(mutex_);
  sqlite3* db = db_.get();
  Transaction tx(db);
  {
    Stmt ins(db,
             "INSERT OR REPLACE INTO messages(id, message_id, in_reply_to, refs, sender, subject, date, flags, added_at)"
             " VALUES(?,?,?,?,?,?,?,?,?)");
    for (const MessageSummary& m : changes.added) {
      ins.bind(1, m.id).bind(2, m.messageId).bind(3, m.inReplyTo).bind(4, joinRefs(m.references));
      ins.bind(5, m.from).bind(6, m.subject).bind(7, static_cast<long long>(m.date)).bind(8, m.flags.toMaildir());
      ins.bind(9, static_cast<long long>(log.at));
      ins.run();
      ins.reset();
    }
    // compare-and-set: если за время синхронизации флаги поменял интерфейс, не затираем их устаревшими
    Stmt upd(db, "UPDATE messages SET flags=? WHERE id=? AND flags=?");
    for (const FlagChange& c : changes.flags) {
      upd.bind(1, c.updated.toMaildir()).bind(2, c.id).bind(3, c.expected.toMaildir());
      upd.run();
      upd.reset();
    }
    Stmt del(db, "DELETE FROM messages WHERE id=?");
    for (const StoreId& id : changes.removed) {
      del.bind(1, id);
      del.run();
      del.reset();
    }
    Stmt logst(db, "INSERT INTO sync_log(at, added, removed, total) VALUES(?,?,?,?)");
    logst.bind(1, static_cast<long long>(log.at)).bind(2, log.added).bind(3, log.removed).bind(4, log.total);
    logst.run();
  }
  tx.commit();
}

void SqliteIndex::updateFlags(const StoreId& id, Flags flags) {
  std::lock_guard lock(mutex_);
  Stmt st(db_.get(), "UPDATE messages SET flags=? WHERE id=?");
  st.bind(1, flags.toMaildir()).bind(2, id);
  st.run();
}

std::vector<SyncLogEntry> SqliteIndex::syncHistory(size_t limit) const {
  std::lock_guard lock(mutex_);
  Stmt st(db_.get(), "SELECT at, added, removed, total FROM sync_log ORDER BY id DESC LIMIT ?");
  st.bind(1, static_cast<long long>(limit));
  std::vector<SyncLogEntry> out;
  while (st.step()) {
    SyncLogEntry e;
    e.at = static_cast<std::time_t>(st.integer(0));
    e.added = static_cast<int>(st.integer(1));
    e.removed = static_cast<int>(st.integer(2));
    e.total = static_cast<int>(st.integer(3));
    out.push_back(e);
  }
  return out;
}

std::unique_ptr<IMessageIndex> openIndex(const std::string& dbPath) {
  try {
    auto idx = std::make_unique<SqliteIndex>(dbPath);
    idx->knownFlags();  // проверка, что файл действительно читается как SQLite
    return idx;
  } catch (const StoreError&) {
    if (dbPath == ":memory:") throw;
    std::error_code ec;
    std::filesystem::rename(dbPath, dbPath + ".broken", ec);  // кэш одноразовый: пересоберём из писем
    if (ec) throw;
    return std::make_unique<SqliteIndex>(dbPath);
  }
}

}  // namespace mail
