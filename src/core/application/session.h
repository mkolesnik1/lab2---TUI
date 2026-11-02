#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/application/content_handlers.h"
#include "core/application/message_renderer.h"
#include "core/application/sync_service.h"
#include "core/domain/charset.h"
#include "core/domain/filters.h"
#include "core/domain/mail_store.h"
#include "core/domain/message_index.h"
#include "core/domain/sorting.h"
#include "core/domain/threading.h"

namespace mail {

enum class Mode { List, Message, Help };

using ListRow = ThreadRow;  // index в messages() или -1 (заглушка треда), depth — отступ дерева

struct StatusMessage {
  std::string text;
  bool error = false;
};

// Состояние сеанса. UI ничего не хранит: всё (хранилище, список, выбранное письмо,
// сортировка, фильтр, режим, прокрутка) — здесь, поэтому сеанс тестируется без терминала.
class MailboxSession {
 public:
  struct Services {
    const ContentHandlerRegistry& handlers;
    const SortRegistry& sorts;
    const FilterRegistry& filters;
  };

  MailboxSession(IMailStore& store, IMessageIndex& index, const ICharsetConverter& conv, Services services);
  MailboxSession(const MailboxSession&) = delete;
  MailboxSession& operator=(const MailboxSession&) = delete;

  // ---- чтение состояния (для представлений) ----
  Mode mode() const { return mode_; }
  const std::vector<MessageSummary>& messages() const { return summaries_; }
  const std::vector<ListRow>& rows() const { return rows_; }
  size_t cursor() const { return cursor_; }
  const MessageSummary* selected() const;  // nullptr: пусто или «заглушка» треда
  const RenderedMessage* message() const { return rendered_ ? &*rendered_ : nullptr; }
  size_t scrollOffset() const { return scroll_; }
  bool showParts() const { return showParts_; }
  size_t partCursor() const { return partCursor_; }
  const std::string& sortName() const { return sortName_; }
  bool descending() const { return descending_; }
  bool threaded() const { return threaded_; }
  const std::string& filterText() const { return filterText_; }
  int unreadCount() const;
  const StatusMessage& status() const { return status_; }
  bool syncRunning() const { return background_.running(); }
  bool quitRequested() const { return quit_; }
  const std::string& storeName() const { return storeName_; }
  size_t pageSize() const { return pageSize_; }

  // ---- действия (их вызывают команды) ----
  void setPageSize(size_t rows) { pageSize_ = rows > 0 ? rows : 1; }
  void clampScroll(size_t maxScroll) { if (scroll_ > maxScroll) scroll_ = maxScroll; }
  void move(int delta);        // курсор списка / прокрутка письма / курсор частей
  void moveToEdge(bool top);
  void page(int direction);    // +1 вниз, -1 вверх
  void openSelected();
  void back();                 // Message/Help -> предыдущий режим
  void showHelp();
  void toggleFlagged();
  void toggleSeen();
  void setSort(const std::string& name);
  void toggleReverse();
  void toggleThreads();
  void setFilter(const std::string& query);
  void togglePartsView();
  void requestQuit() { quit_ = true; }
  void setStatus(std::string text, bool error = false);

  // ---- синхронизация ----
  void start();            // показать кэш из индекса и запустить фоновую синхронизацию
  void refresh();          // кнопка R: фоновая синхронизация
  void tick();             // вызывается циклом UI: забирает результат фоновой синхронизации
  SyncResult syncNow();    // блокирующая синхронизация (режим --list, тесты)
  void reloadFromIndex();

  // ---- данные для действий над письмом ----
  const PartEntry* targetAttachment() const;    // часть под курсором / первое вложение
  std::string suggestedAttachmentName() const;
  std::string currentBodyText() const;
  std::string currentRaw() const;               // сырые байты письма (для .eml)
  std::string suggestedBaseName() const;

 private:
  template <class F>
  void guard(F&& f);
  void rebuildRows();
  void applyFlags(size_t summaryIndex, Flags flags);
  void closeMessage();

  IMailStore& store_;
  IMessageIndex& index_;
  const ICharsetConverter& conv_;
  Services services_;
  MessageRenderer renderer_;
  SyncService syncService_;
  BackgroundSync background_;

  std::string storeName_;
  std::vector<MessageSummary> summaries_;
  std::vector<ListRow> rows_;
  size_t cursor_ = 0;

  Mode mode_ = Mode::List;
  Mode previousMode_ = Mode::List;
  std::unique_ptr<Message> current_;
  std::optional<RenderedMessage> rendered_;
  size_t scroll_ = 0;
  size_t savedScroll_ = 0;  // прокрутка письма на время показа справки
  StoreId openId_;          // id открытого письма
  bool showParts_ = false;
  size_t partCursor_ = 0;

  std::string sortName_ = "date";
  bool descending_ = true;
  bool threaded_ = true;
  std::string filterText_;
  std::unique_ptr<IFilter> filter_;

  size_t pageSize_ = 20;
  StatusMessage status_;
  bool quit_ = false;
};

}  // namespace mail
