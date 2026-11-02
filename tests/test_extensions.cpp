// Каждый тест здесь — доказательство точки расширения: новый элемент добавляется НОВЫМ классом
// и регистрацией, без правки существующих классов ядра.
#include <map>

#include "check.h"
#include "core/application/key_bindings.h"
#include "core/application/store_registry.h"
#include "core/domain/errors.h"
#include "env.h"

using namespace mail;
using namespace testutil;

namespace {

// --- новое хранилище: письма в памяти (прообраз mbox / IMAP только на чтение) ---
class MemoryStore final : public IMailStore {
 public:
  explicit MemoryStore(bool readOnly) : readOnly_(readOnly) {
    raw_["a"] = "From: Mem <m@x>\nSubject: In memory\nDate: Mon, 05 Oct 2026 10:00:00 +0000\nMessage-ID: <a@m>\n\nstored in RAM\n";
    raw_["b"] = "From: Mem <m@x>\nSubject: Re: In memory\nIn-Reply-To: <a@m>\nDate: Tue, 06 Oct 2026 10:00:00 +0000\n\nreply\n";
  }
  std::string name() const override { return "memory"; }
  std::vector<StoredMessage> list() const override {
    std::vector<StoredMessage> out;
    for (const auto& [id, raw] : raw_) out.push_back({id, flags_.count(id) ? flags_.at(id) : Flags{}});
    return out;
  }
  std::string readRaw(const StoreId& id) const override {
    auto it = raw_.find(id);
    if (it == raw_.end()) throw StoreError("нет письма " + id);
    return it->second;
  }
  void setFlags(const StoreId& id, Flags f) override {
    if (readOnly_) throw StoreError("хранилище только для чтения");
    flags_[id] = f;
  }

 private:
  bool readOnly_;
  std::map<StoreId, std::string> raw_;
  std::map<StoreId, Flags> flags_;
};

// --- новый обработчик типа содержимого ---
class CsvHandler final : public IContentHandler {
 public:
  bool canHandle(const std::string& mime) const override { return mime == "text/csv"; }
  int priority() const override { return 20; }
  std::string render(const LeafPart& part, const ICharsetConverter& conv) const override {
    return "Таблица: " + decodeTextBody(part, conv);
  }
};

// --- новый критерий сортировки ---
class LengthSort final : public ISortStrategy {
 public:
  std::string name() const override { return "subject-length"; }
  bool less(const MessageSummary& a, const MessageSummary& b) const override {
    return a.subject.size() != b.subject.size() ? a.subject.size() < b.subject.size() : a.id < b.id;
  }
  bool defaultDescending() const override { return false; }
};

// --- новый критерий фильтрации ---
class HasReferencesFilter final : public IFilter {
 public:
  bool matches(const MessageSummary& m) const override { return !m.references.empty() || !m.inReplyTo.empty(); }
};

// --- новая команда ---
class CountCommand final : public ICommand {
 public:
  std::string name() const override { return "count"; }
  std::string description() const override { return "Показать число писем"; }
  void execute(MailboxSession& s, const std::string&) const override {
    s.setStatus("Писем: " + std::to_string(s.messages().size()));
  }
};

}  // namespace

TEST(ext, new_store_registered_by_key_and_used_through_the_same_session) {
  StoreRegistry stores;
  stores.add("memory", [](const std::string& loc) { return std::make_unique<MemoryStore>(loc == "ro"); });
  CHECK_THROWS(stores.create("mbox", "x"), StoreError);  // неизвестный тип -> понятная ошибка

  Env env;
  auto store = stores.create("memory", "rw");
  SqliteIndex index(":memory:");
  MailboxSession session(*store, index, env.conv, env.services());
  const SyncResult r = session.syncNow();
  CHECK_EQ(r.added, 2);
  CHECK_EQ(session.rows().size(), size_t{2});
  CHECK_EQ(session.rows()[1].depth, 1);  // тред собран так же, как для Maildir
  session.openSelected();
  CHECK(session.message() != nullptr);
}

TEST(ext, read_only_store_reports_error_but_message_still_opens) {
  Env env;
  MemoryStore store(true);
  SqliteIndex index(":memory:");
  MailboxSession session(store, index, env.conv, env.services());
  session.syncNow();
  session.openSelected();  // пометить прочитанным нельзя, но читать можно
  CHECK(session.mode() == Mode::Message);
  CHECK(session.status().error);
}

TEST(ext, new_content_handler_is_chosen_by_priority_without_touching_existing_code) {
  Env env;
  env.handlers.add(std::make_unique<CsvHandler>());
  const Message m = MimeParser::parse("Content-Type: text/csv\n\na,b\n");
  MessageRenderer renderer(env.handlers, env.conv);
  CHECK(renderer.render(m).body.find("Таблица: a,b") != std::string::npos);

  // text/plain всё равно предпочтительнее (priority 0 < 20)
  const Message alt = MimeParser::parse(
      "Content-Type: multipart/alternative; boundary=B\n\n--B\nContent-Type: text/csv\n\ncal\n--B\n"
      "Content-Type: text/plain\n\nplain\n--B--\n");
  CHECK_EQ(renderer.render(alt).body, std::string("plain"));
}

TEST(ext, new_sort_strategy_and_filter_are_registered_by_name) {
  Env env;
  env.sorts.add(std::make_unique<LengthSort>());
  env.filters.add("reply", [](const std::string&) { return std::make_unique<HasReferencesFilter>(); });

  MemoryStore store(false);
  SqliteIndex index(":memory:");
  MailboxSession session(store, index, env.conv, env.services());
  session.syncNow();
  session.toggleThreads();
  session.setSort("subject-length");
  CHECK_EQ(session.messages()[static_cast<size_t>(session.rows()[0].index)].subject, std::string("In memory"));
  session.setFilter("reply:");
  CHECK_EQ(session.rows().size(), size_t{1});
}

TEST(ext, new_command_is_registered_and_bound_to_a_key_from_config) {
  Env env;
  env.commands.add(std::make_unique<CountCommand>());
  KeyBindings keys = KeyBindings::withDefaults();
  std::istringstream cfg("n = count\n");
  CHECK(keys.load(cfg, env.commands).empty());

  MemoryStore store(false);
  SqliteIndex index(":memory:");
  MailboxSession session(store, index, env.conv, env.services());
  session.syncNow();
  runCommand(*env.commands.find(*keys.commandFor("n")), session, "");
  CHECK_EQ(session.status().text, std::string("Писем: 2"));
}
