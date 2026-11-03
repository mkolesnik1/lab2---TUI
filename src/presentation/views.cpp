#include "presentation/views.h"

#include <algorithm>
#include <ctime>

#include "core/domain/summary.h"
#include "core/domain/text_util.h"
#include "presentation/terminal.h"

namespace mail {
namespace {

std::string flagsColumn(const Flags& f) {
  std::string s = "    ";
  if (!f.seen) s[0] = 'N';
  if (f.flagged) s[1] = '!';
  if (f.replied) s[2] = 'r';
  if (f.trashed) s[3] = 'T';
  else if (f.draft) s[3] = 'D';
  return s;
}

std::string dateColumn(std::time_t t) {
  if (t == 0) return "     —     ";
  char buf[32];
  std::tm tmv{};
  localtime_r(&t, &tmv);
  std::strftime(buf, sizeof buf, "%d.%m %H:%M", &tmv);
  return buf;
}

// Строка-разделитель: подпись и «─» до точной ширины окна (без обрезки «…»).
std::string ruler(const std::string& label, int width) {
  int used = 0;
  for (char32_t cp : decodeUtf8(label)) used += cellWidth(cp);
  std::string out = label;
  for (int i = used; i < width; ++i) out += "─";
  return out;
}

std::string repeat(const std::string& s, int n) {
  std::string out;
  for (int i = 0; i < n; ++i) out += s;
  return out;
}

}  // namespace

// ---- список писем ---------------------------------------------------------------

void MailListView::draw(MailboxSession& s, const Rect& a) {
  const int listRows = a.h - 1;
  s.setPageSize(static_cast<size_t>(std::max(listRows, 1)));
  putText(a.y, a.x,
          " maills — " + s.storeName() + "   писем: " + std::to_string(s.messages().size()) +
              ", непрочитанных: " + std::to_string(s.unreadCount()),
          a.w, A_REVERSE);

  const auto& rows = s.rows();
  if (rows.empty()) {
    putText(a.y + 1, a.x, s.syncRunning() ? " Загрузка писем..." : " Писем нет (R — обновить)", a.w);
    return;
  }
  const size_t visible = static_cast<size_t>(std::max(listRows, 1));
  if (s.cursor() < top_) top_ = s.cursor();
  if (s.cursor() >= top_ + visible) top_ = s.cursor() - visible + 1;

  const int senderW = std::clamp(a.w / 4, 8, 24);
  for (int i = 0; i < listRows; ++i) {
    const size_t r = top_ + static_cast<size_t>(i);
    if (r >= rows.size()) break;
    const ListRow& row = rows[r];
    const int indent = std::min(row.depth, 6);
    const std::string prefix = row.depth > 0 ? repeat("  ", indent - 1) + "└─ " : "";
    std::string line;
    int attrs = 0;
    if (row.index < 0) {
      line = "     " + std::string(11, ' ') + " " + std::string(static_cast<size_t>(senderW), ' ') + " " + prefix +
             "(письмо отсутствует в ящике)";
    } else {
      const MessageSummary& m = s.messages()[static_cast<size_t>(row.index)];
      line = flagsColumn(m.flags) + " " + dateColumn(m.date) + " " +
             fitToWidth(shortSender(m.from), senderW, true) + " " + prefix + (m.subject.empty() ? "(без темы)" : m.subject);
      if (!m.flags.seen) attrs |= A_BOLD;
      if (m.flags.flagged && has_colors()) attrs |= COLOR_PAIR(2);
    }
    if (r == s.cursor()) attrs |= A_REVERSE;
    putText(a.y + 1 + i, a.x, " " + line, a.w, attrs);
  }
}

// ---- просмотр письма ---------------------------------------------------------------

void MessageView::draw(MailboxSession& s, const Rect& a) {
  const RenderedMessage* m = s.message();
  if (!m) {
    putText(a.y, a.x, " Письмо не открыто", a.w);
    return;
  }
  int y = a.y;
  for (const auto& [name, value] : m->headers) {
    if (y >= a.y + a.h - 2) break;  // оставляем место под разделитель и хотя бы строку текста
    putText(y++, a.x, name + ": " + value, a.w, A_BOLD);
  }
  const int content = a.y + a.h - (y + 1);
  s.setPageSize(static_cast<size_t>(std::max(content, 1)));

  if (s.showParts()) {
    putText(y, a.x, ruler("── Части письма (↑/↓ — выбор, s — сохранить, p — назад к тексту) ", a.w), a.w);
    const auto& parts = m->parts;
    const size_t visible = static_cast<size_t>(std::max(content, 1));
    if (s.partCursor() < partsTop_) partsTop_ = s.partCursor();
    if (s.partCursor() >= partsTop_ + visible) partsTop_ = s.partCursor() - visible + 1;
    for (int i = 0; i < content; ++i) {
      const size_t idx = partsTop_ + static_cast<size_t>(i);
      if (idx >= parts.size()) break;
      const PartEntry& p = parts[idx];
      const std::string mark = p.attachment ? "[вложение] " : "";
      putText(y + 1 + i, a.x, " " + repeat("  ", p.depth) + mark + p.label, a.w, idx == s.partCursor() ? A_REVERSE : 0);
    }
    return;
  }

  const std::vector<std::string> lines = wrapText(m->body, std::max(a.w - 2, 10));
  const size_t visible = static_cast<size_t>(std::max(content, 1));
  const size_t maxScroll = lines.size() > visible ? lines.size() - visible : 0;
  s.clampScroll(maxScroll);
  const size_t first = s.scrollOffset();
  const size_t last = std::min(lines.size(), first + visible);
  putText(y, a.x,
          ruler("── Текст " + std::to_string(lines.empty() ? 0 : first + 1) + "–" + std::to_string(last) + "/" +
                    std::to_string(lines.size()) + "  (p — части письма) ",
                a.w),
          a.w);
  for (size_t i = first; i < last; ++i) putText(y + 1 + static_cast<int>(i - first), a.x, " " + lines[i], a.w);
}

// ---- справка ------------------------------------------------------------------------

void HelpView::draw(MailboxSession& s, const Rect& a) {
  std::vector<std::string> lines;
  lines.push_back("Свои клавиши: ~/.config/maills/keys.conf, строки «клавиша = команда».");
  lines.push_back("");
  for (const ICommand* c : commands_.all()) {
    std::string keys;
    for (const std::string& k : keys_.keysFor(c->name())) keys += (keys.empty() ? "" : ", ") + k;
    if (keys.empty()) keys = "(не назначена)";
    lines.push_back(fitToWidth(keys, 24, true) + " " + c->description());
  }
  putText(a.y, a.x, " Справка  (q или Esc — назад)", a.w, A_REVERSE);
  const size_t visible = static_cast<size_t>(std::max(a.h - 1, 1));
  s.setPageSize(visible);
  s.clampScroll(lines.size() > visible ? lines.size() - visible : 0);
  for (size_t i = 0; i < visible && s.scrollOffset() + i < lines.size(); ++i)
    putText(a.y + 1 + static_cast<int>(i), a.x, " " + lines[s.scrollOffset() + i], a.w);
}

// ---- строка состояния -------------------------------------------------------------

void drawStatusBar(const MailboxSession& s, const std::optional<PromptView>& prompt, int y, int width) {
  if (prompt) {
    putText(y, 0, " " + prompt->label + prompt->text + "_", width - 1, A_BOLD);
    return;
  }
  std::string left;
  switch (s.mode()) {
    case Mode::List: left = "[СПИСОК]"; break;
    case Mode::Message: left = "[ПИСЬМО]"; break;
    case Mode::Help: left = "[СПРАВКА]"; break;
  }
  left += " " + s.sortName() + (s.descending() ? "↓" : "↑") + (s.threaded() ? " треды" : " плоско");
  if (!s.filterText().empty()) left += " /" + s.filterText();
  if (s.mode() == Mode::List && !s.rows().empty()) left += " " + std::to_string(s.cursor() + 1) + "/" + std::to_string(s.rows().size());
  if (s.syncRunning()) left += " ⟳";
  const std::string text = left + " │ " + s.status().text;
  const bool err = s.status().error;
  putText(y, 0, " " + text, width - 1, err && has_colors() ? (COLOR_PAIR(1) | A_BOLD) : A_REVERSE);
}

}  // namespace mail
