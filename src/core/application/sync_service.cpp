#include "core/application/sync_service.h"

#include <chrono>
#include <unordered_set>

#include "core/domain/errors.h"
#include "core/domain/mime.h"
#include "core/domain/summary.h"

namespace mail {

SyncResult SyncService::run() {
  const std::vector<StoredMessage> stored = store_.list();
  const std::unordered_map<StoreId, Flags> known = index_.knownFlags();

  IndexChanges changes;
  SyncResult result;
  std::unordered_set<StoreId> present;

  for (const StoredMessage& s : stored) {
    present.insert(s.id);
    auto it = known.find(s.id);
    if (it == known.end()) {
      try {
        changes.added.push_back(makeSummary(s.id, s.flags, MimeParser::parseHeaders(store_.readRaw(s.id)), conv_));
      } catch (const ParseError& e) {  // битое письмо всё равно показываем в списке
        MessageSummary broken;
        broken.id = s.id;
        broken.flags = s.flags;
        broken.subject = std::string("(не удалось разобрать: ") + e.what() + ")";
        changes.added.push_back(std::move(broken));
        ++result.failed;
      } catch (const StoreError&) {  // файл исчез между list() и read() — подхватим при следующей синхронизации
        ++result.failed;
      }
    } else if (!(it->second == s.flags)) {
      changes.flags.push_back({s.id, it->second, s.flags});
    }
  }
  for (const auto& [id, flags] : known) {
    (void)flags;
    if (!present.count(id)) changes.removed.push_back(id);
  }

  result.added = static_cast<int>(changes.added.size());
  result.removed = static_cast<int>(changes.removed.size());
  result.flagsChanged = static_cast<int>(changes.flags.size());
  result.total = static_cast<int>(known.size()) + result.added - result.removed;

  SyncLogEntry log;
  log.at = std::time(nullptr);
  log.added = result.added;
  log.removed = result.removed;
  log.total = result.total;
  index_.apply(changes, log);  // одна транзакция

  for (const MessageSummary& m : index_.loadAll())
    if (!m.flags.seen) ++result.unread;
  return result;
}

bool BackgroundSync::running() const {
  return future_.valid() && future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}

bool BackgroundSync::start() {
  if (future_.valid()) return false;
  future_ = std::async(std::launch::async, [this] { return service_.run(); });
  return true;
}

std::optional<BackgroundSync::Outcome> BackgroundSync::poll() {
  if (!future_.valid() || future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return std::nullopt;
  Outcome o;
  try {
    o.result = future_.get();
  } catch (const std::exception& e) {
    o.error = e.what();
  }
  return o;
}

}  // namespace mail
