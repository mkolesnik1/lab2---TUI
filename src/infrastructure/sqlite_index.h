#pragma once
#include <memory>
#include <mutex>
#include <string>

#include "core/domain/message_index.h"

struct sqlite3;

namespace mail {

// Индекс заголовков в SQLite (одна таблица messages + журнал sync_log). Это кэш:
// его можно удалить — он пересоберётся из хранилища. Все методы под мьютексом,
// потому что синхронизация идёт в фоновом потоке, а интерфейс читает из основного.
class SqliteIndex final : public IMessageIndex {
 public:
  explicit SqliteIndex(const std::string& dbPath);  // ":memory:" допустим; StoreError при сбое
  ~SqliteIndex() override;
  SqliteIndex(const SqliteIndex&) = delete;
  SqliteIndex& operator=(const SqliteIndex&) = delete;

  std::vector<MessageSummary> loadAll() const override;
  std::unordered_map<StoreId, Flags> knownFlags() const override;
  void apply(const IndexChanges& changes, const SyncLogEntry& log) override;
  void updateFlags(const StoreId& id, Flags flags) override;
  std::vector<SyncLogEntry> syncHistory(size_t limit) const override;

 private:
  struct Closer {
    void operator()(sqlite3* db) const noexcept;
  };
  std::unique_ptr<sqlite3, Closer> db_;
  mutable std::mutex mutex_;
};

// Открывает индекс; если файл повреждён (не SQLite) — переименовывает его в *.broken и создаёт заново.
std::unique_ptr<IMessageIndex> openIndex(const std::string& dbPath);

}  // namespace mail
