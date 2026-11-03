#pragma once
#include <ncurses.h>

#include <optional>
#include <string>

namespace mail {

struct Key {
  bool isCode = false;  // функциональная клавиша (стрелки, PageUp...) или обычный символ
  wint_t value = 0;
};

// RAII над ncurses: initscr в конструкторе, endwin в деструкторе (терминал восстановится даже при исключении).
class Terminal {
 public:
  Terminal();
  ~Terminal();
  Terminal(const Terminal&) = delete;
  Terminal& operator=(const Terminal&) = delete;

  std::optional<Key> readKey() const;  // ждёт не дольше 100 мс, чтобы цикл мог опрашивать фоновую синхронизацию
};

// "q", "Down", "Enter", "Space", "PageUp"...; "" — клавиша без имени.
std::string keyName(const Key& key);
// Печатный символ в UTF-8 для строки ввода; "" для управляющих клавиш.
std::string keyText(const Key& key);

// Рисование строки в ячейках терминала: обрезка и добивка по ширине выполняются в ядре (fitToWidth).
void putText(int y, int x, const std::string& text, int width, int attrs = 0);

}  // namespace mail
