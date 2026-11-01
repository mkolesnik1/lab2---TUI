#pragma once
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/domain/flags.h"
#include "core/domain/summary.h"

namespace mail {

struct FlagChange {
  StoreId id;
  Flags expected;  // что индекс знал при сравнении
  Flags updated;   // что сейчас в хранилище
};

struct IndexChanges {
  std::vector<MessageSummary> added;
  std::vector<FlagChange> flags;
  std::vector<StoreId> removed;
};

struct SyncLogEntry {
  std::time_t at = 0;
  int added = 0;
  int removed = 0;
  int total = 0;
};

// Порт индекса (Repository). Это КЭШ: источник истины — хранилище (имена файлов Maildir).
// Реализация — SQLite (infrastructure).
class IMessageIndex {
 public:
  virtual ~IMessageIndex() = default;
  virtual std::vector<MessageSummary> loadAll() const = 0;
  virtual std::unordered_map<StoreId, Flags> knownFlags() const = 0;
  // Атомарно: либо применены все изменения и запись журнала, либо ничего.
  // Смена флагов выполняется как compare-and-set (только если у записи ещё flags == expected).
  virtual void apply(const IndexChanges& changes, const SyncLogEntry& log) = 0;
  virtual void updateFlags(const StoreId& id, Flags flags) = 0;
  virtual std::vector<SyncLogEntry> syncHistory(size_t limit) const = 0;  // новые первыми
};

}  // namespace mail
