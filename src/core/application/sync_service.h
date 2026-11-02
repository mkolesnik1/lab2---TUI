#pragma once
#include <future>
#include <optional>
#include <string>

#include "core/domain/charset.h"
#include "core/domain/mail_store.h"
#include "core/domain/message_index.h"

namespace mail {

struct SyncResult {
  int added = 0;
  int removed = 0;
  int flagsChanged = 0;
  int failed = 0;   // письма, у которых не удалось прочитать заголовки
  int total = 0;
  int unread = 0;
};

// Сверяет хранилище (истина) с индексом (кэш): новые файлы -> разбор заголовков,
// исчезнувшие -> удаление, изменённые флаги -> обновление. Ручные правки Maildir подхватываются.
class SyncService {
 public:
  SyncService(IMailStore& store, IMessageIndex& index, const ICharsetConverter& conv)
      : store_(store), index_(index), conv_(conv) {}
  SyncResult run();

 private:
  IMailStore& store_;
  IMessageIndex& index_;
  const ICharsetConverter& conv_;
};

// Фоновая синхронизация: интерфейс не блокируется, а раз в цикл спрашивает poll().
class BackgroundSync {
 public:
  struct Outcome {
    std::optional<SyncResult> result;
    std::string error;  // непусто, если синхронизация упала
  };
  explicit BackgroundSync(SyncService& service) : service_(service) {}
  bool running() const;
  bool start();                  // false, если уже идёт
  std::optional<Outcome> poll();  // готовый результат (один раз) или nullopt

 private:
  SyncService& service_;
  std::future<SyncResult> future_;
};

}  // namespace mail
