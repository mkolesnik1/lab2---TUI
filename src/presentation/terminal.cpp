#include "presentation/terminal.h"

#include "core/domain/text_util.h"

namespace mail {

Terminal::Terminal() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  set_escdelay(25);
  timeout(100);
  if (has_colors()) {
    start_color();
    use_default_colors();
    init_pair(1, COLOR_WHITE, COLOR_RED);  // ошибки
    init_pair(2, COLOR_CYAN, -1);          // пометка
  }
}

Terminal::~Terminal() { endwin(); }

std::optional<Key> Terminal::readKey() const {
  wint_t ch = 0;
  const int r = get_wch(&ch);
  if (r == ERR) return std::nullopt;
  return Key{r == KEY_CODE_YES, ch};
}

std::string keyName(const Key& key) {
  if (key.isCode) {
    switch (key.value) {
      case KEY_UP: return "Up";
      case KEY_DOWN: return "Down";
      case KEY_LEFT: return "Left";
      case KEY_RIGHT: return "Right";
      case KEY_NPAGE: return "PageDown";
      case KEY_PPAGE: return "PageUp";
      case KEY_HOME: return "Home";
      case KEY_END: return "End";
      case KEY_BACKSPACE: return "Backspace";
      case KEY_ENTER: return "Enter";
      case KEY_F(1): return "F1";
      default: return "";
    }
  }
  switch (key.value) {
    case 10:
    case 13: return "Enter";
    case 27: return "Esc";
    case 9: return "Tab";
    case 8:
    case 127: return "Backspace";
    case ' ': return "Space";
    default: break;
  }
  return keyText(key);
}

std::string keyText(const Key& key) {
  if (key.isCode || key.value < 32 || key.value == 127) return {};
  std::string s;
  appendUtf8(s, static_cast<char32_t>(key.value));
  return s;
}

void putText(int y, int x, const std::string& text, int width, int attrs) {
  const std::string fitted = fitToWidth(text, width, true);
  attron(attrs);
  mvaddstr(y, x, fitted.c_str());
  attroff(attrs);
}

}  // namespace mail
