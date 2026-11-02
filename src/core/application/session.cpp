#include "core/application/session.h"

#include <algorithm>
#include <cstdio>

#include "core/domain/errors.h"
#include "core/domain/mime.h"
#include "core/domain/text_util.h"

namespace mail {

MailboxSession::MailboxSession(IMailStore& store, IMessageIndex& index, const ICharsetConverter& conv,
                               Services services)
    : store_(store),
      index_(index),
      conv_(conv),
      services_(services),
      renderer_(services.handlers, conv),
      syncService_(store, index, conv),
      background_(syncService_),
      storeName_(store.name()) {}

template <class F>
void MailboxSession::guard(F&& f) {
  try {
    f();
  } catch (const MailError& e) {
    setStatus(e.what(), true);
  } catch (const std::exception& e) {
    setStatus(std::string("внутренняя ошибка: ") + e.what(), true);
  }
}

void MailboxSession::setStatus(std::string text, bool error) { status_ = StatusMessage{std::move(text), error}; }

const MessageSummary* MailboxSession::selected() const {
  if (cursor_ >= rows_.size() || rows_[cursor_].index < 0) return nullptr;
  return &summaries_[static_cast<size_t>(rows_[cursor_].index)];
}

int MailboxSession::unreadCount() const {
  return static_cast<int>(std::count_if(summaries_.begin(), summaries_.end(),
                                        [](const MessageSummary& m) { return !m.flags.seen; }));
}

// ---- список -------------------------------------------------------------

void MailboxSession::rebuildRows() {
  StoreId keep;
  if (const MessageSummary* s = selected()) keep = s->id;

  rows_.clear();
  const ISortStrategy* sort = services_.sorts.find(sortName_);
  if (!sort) sort = services_.sorts.find("date");
  const int n = static_cast<int>(summaries_.size());

  if (threaded_ && !filter_ && sort) {
    rows_ = ThreadBuilder::flatten(ThreadBuilder::build(summaries_, *sort, descending_));
  } else {  // плоский список (при поиске деревья не строим)
    std::vector<int> idx;
    for (int i = 0; i < n; ++i)
      if (!filter_ || filter_->matches(summaries_[static_cast<size_t>(i)])) idx.push_back(i);
    if (sort)
      std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
        const auto& x = summaries_[static_cast<size_t>(a)];
        const auto& y = summaries_[static_cast<size_t>(b)];
        return descending_ ? sort->less(y, x) : sort->less(x, y);
      });
    for (int i : idx) rows_.push_back({i, 0});
  }

  cursor_ = std::min(cursor_, rows_.empty() ? size_t{0} : rows_.size() - 1);
  if (!keep.empty())
    for (size_t r = 0; r < rows_.size(); ++r)
      if (rows_[r].index >= 0 && summaries_[static_cast<size_t>(rows_[r].index)].id == keep) {
        cursor_ = r;
        break;
      }
}

void MailboxSession::move(int delta) {
  const auto step = [delta](size_t value, size_t limit) {  // сдвиг с зажатием в [0, limit]
    const long long v = static_cast<long long>(value) + delta;
    return static_cast<size_t>(std::clamp<long long>(v, 0, static_cast<long long>(limit)));
  };
  switch (mode_) {
    case Mode::List:
      if (!rows_.empty()) cursor_ = step(cursor_, rows_.size() - 1);
      break;
    case Mode::Message:
      if (showParts_ && rendered_) {
        if (!rendered_->parts.empty()) partCursor_ = step(partCursor_, rendered_->parts.size() - 1);
      } else {
        scroll_ = step(scroll_, size_t{1} << 30);  // верхнюю границу ограничит представление
      }
      break;
    case Mode::Help:
      scroll_ = step(scroll_, size_t{1} << 30);
      break;
  }
}

void MailboxSession::moveToEdge(bool top) {
  const size_t big = size_t{1} << 30;
  switch (mode_) {
    case Mode::List:
      cursor_ = top || rows_.empty() ? 0 : rows_.size() - 1;
      break;
    case Mode::Message:
      if (showParts_ && rendered_) partCursor_ = top || rendered_->parts.empty() ? 0 : rendered_->parts.size() - 1;
      else scroll_ = top ? 0 : big;
      break;
    case Mode::Help:
      scroll_ = top ? 0 : big;
      break;
  }
}

void MailboxSession::page(int direction) {
  const int amount = static_cast<int>(pageSize_ > 1 ? pageSize_ - 1 : 1);
  move(direction * amount);
}

// ---- письмо ----------------------------------------------------------------

void MailboxSession::closeMessage() {
  rendered_.reset();  // PartEntry указывают внутрь current_ — сначала освобождаем их
  current_.reset();
  openId_.clear();
}

void MailboxSession::openSelected() {
  if (mode_ != Mode::List) return;
  const MessageSummary* s = selected();
  if (!s) return;
  const StoreId id = s->id;
  try {
    auto msg = std::make_unique<Message>(MimeParser::parse(store_.readRaw(id)));
    RenderedMessage rendered = renderer_.render(*msg);
    closeMessage();
    current_ = std::move(msg);
    rendered_ = std::move(rendered);
  } catch (const MailError& e) {  // битое письмо не роняет программу
    setStatus(std::string("Не удалось открыть письмо: ") + e.what(), true);
    return;
  }
  openId_ = id;
  scroll_ = 0;
  showParts_ = false;
  partCursor_ = 0;
  previousMode_ = Mode::List;
  mode_ = Mode::Message;
  status_ = {};

  if (!selected()->flags.seen) {  // открыли -> «прочитано» (в Maildir: письмо уходит в cur/ и получает S)
    Flags f = selected()->flags;
    f.seen = true;
    try {
      applyFlags(static_cast<size_t>(rows_[cursor_].index), f);
    } catch (const MailError& e) {
      setStatus(std::string("Не удалось отметить прочитанным: ") + e.what(), true);
    }
  }
}

void MailboxSession::back() {
  if (mode_ == Mode::Message) {
    closeMessage();
    mode_ = Mode::List;
  } else if (mode_ == Mode::Help) {
    mode_ = previousMode_;
    scroll_ = savedScroll_;
  }
}

void MailboxSession::showHelp() {
  if (mode_ == Mode::Help) return;
  previousMode_ = mode_;
  savedScroll_ = scroll_;
  scroll_ = 0;
  mode_ = Mode::Help;
}

void MailboxSession::togglePartsView() {
  if (mode_ == Mode::Message) showParts_ = !showParts_;
}

// ---- флаги -----------------------------------------------------------------

void MailboxSession::applyFlags(size_t i, Flags flags) {
  const StoreId id = summaries_[i].id;
  store_.setFlags(id, flags);  // источник истины — хранилище (в Maildir это rename файла)
  summaries_[i].flags = flags;
  index_.updateFlags(id, flags);  // кэш; если он отстал — следующая синхронизация исправит
}

void MailboxSession::toggleFlagged() {
  if (mode_ == Mode::Help || !selected()) return;
  guard([&] {
    Flags f = selected()->flags;
    f.flagged = !f.flagged;
    applyFlags(static_cast<size_t>(rows_[cursor_].index), f);
    setStatus(f.flagged ? "Письмо помечено" : "Пометка снята");
  });
}

void MailboxSession::toggleSeen() {
  if (mode_ == Mode::Help || !selected()) return;
  guard([&] {
    Flags f = selected()->flags;
    f.seen = !f.seen;
    applyFlags(static_cast<size_t>(rows_[cursor_].index), f);
    setStatus(f.seen ? "Отмечено прочитанным" : "Отмечено непрочитанным");
  });
}

// ---- сортировка и поиск ----------------------------------------------------------

void MailboxSession::setSort(const std::string& name) {
  const ISortStrategy* s = services_.sorts.find(name);
  if (!s) {
    setStatus("Неизвестный критерий сортировки: " + name, true);
    return;
  }
  sortName_ = name;
  descending_ = s->defaultDescending();
  rebuildRows();
  setStatus("Сортировка: " + name);
}

void MailboxSession::toggleReverse() {
  descending_ = !descending_;
  rebuildRows();
}

void MailboxSession::toggleThreads() {
  threaded_ = !threaded_;
  rebuildRows();
  setStatus(threaded_ ? "Треды включены" : "Треды выключены");
}

void MailboxSession::setFilter(const std::string& query) {
  filter_ = services_.filters.create(query);
  filterText_ = filter_ ? std::string(trim(query)) : std::string();
  rebuildRows();
  setStatus(filter_ ? "Найдено писем: " + std::to_string(rows_.size()) : "Фильтр снят");
}

// ---- синхронизация -------------------------------------------------------------

void MailboxSession::reloadFromIndex() {
  guard([&] {
    summaries_ = index_.loadAll();
    rebuildRows();
    if (current_) {
      const bool stillThere = std::any_of(summaries_.begin(), summaries_.end(),
                                          [&](const MessageSummary& m) { return m.id == openId_; });
      if (!stillThere) {  // письмо удалили из ящика, пока оно было открыто
        closeMessage();
        mode_ = Mode::List;
      }
    }
  });
}

void MailboxSession::start() {
  reloadFromIndex();
  guard([&] {
    const auto history = index_.syncHistory(1);
    if (history.empty()) {
      setStatus("Первая синхронизация...");
    } else {
      char when[32] = "";
      const std::time_t t = history[0].at;
      std::tm tmv{};
      localtime_r(&t, &tmv);
      std::strftime(when, sizeof when, "%d.%m %H:%M", &tmv);
      setStatus("Из кэша: " + std::to_string(summaries_.size()) + " писем (синхронизация " + when + "). Обновляю...");
    }
  });
  background_.start();
}

void MailboxSession::refresh() {
  if (background_.start()) setStatus("Обновляю...");
  else setStatus("Синхронизация уже идёт");
}

void MailboxSession::tick() {
  auto outcome = background_.poll();
  if (!outcome) return;
  if (!outcome->error.empty()) {
    setStatus("Ошибка синхронизации: " + outcome->error, true);
    return;
  }
  const SyncResult r = *outcome->result;
  status_ = {};
  reloadFromIndex();
  if (status_.error) return;
  std::string text = "+" + std::to_string(r.added) + " новых, -" + std::to_string(r.removed) + " удалено, всего " +
                     std::to_string(r.total) + ", непрочитанных " + std::to_string(r.unread);
  if (r.flagsChanged > 0) text += ", флагов " + std::to_string(r.flagsChanged);
  if (r.failed > 0) text += ", ошибок чтения " + std::to_string(r.failed);
  setStatus(std::move(text));
}

SyncResult MailboxSession::syncNow() {
  const SyncResult r = syncService_.run();
  reloadFromIndex();
  return r;
}

// ---- данные для команд ---------------------------------------------------------------

const PartEntry* MailboxSession::targetAttachment() const {
  if (!rendered_) return nullptr;
  if (showParts_ && partCursor_ < rendered_->parts.size()) return &rendered_->parts[partCursor_];
  for (const PartEntry& p : rendered_->parts)
    if (p.leaf && p.attachment) return &p;
  return nullptr;
}

std::string MailboxSession::suggestedAttachmentName() const {
  const PartEntry* p = targetAttachment();
  if (!p || !p->leaf) return {};
  if (!p->filename.empty()) return p->filename;
  const std::string mime = p->leaf->mimeType();
  return mime == "text/plain" ? "part.txt" : mime == "text/html" ? "part.html" : "part.bin";
}

std::string MailboxSession::currentBodyText() const {
  if (!rendered_) throw StoreError("письмо не открыто");
  return rendered_->body;
}

std::string MailboxSession::currentRaw() const {
  StoreId id = openId_;
  if (id.empty()) {
    const MessageSummary* s = selected();
    if (!s) throw StoreError("нет выбранного письма");
    id = s->id;
  }
  return store_.readRaw(id);
}

std::string MailboxSession::suggestedBaseName() const {
  const MessageSummary* s = selected();
  if (!s) return "message";
  std::string name = sanitizeFilename(s->subject);
  const auto cps = decodeUtf8(name);
  if (cps.size() > 40) {
    name.clear();
    for (size_t i = 0; i < 40; ++i) appendUtf8(name, cps[i]);
  }
  return name.empty() ? s->id : name;
}

}  // namespace mail
