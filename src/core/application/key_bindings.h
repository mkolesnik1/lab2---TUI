#pragma once
#include <istream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/application/commands.h"

namespace mail {

// Клавиша -> имя команды. Значения по умолчанию + переопределение из конфига.
// Формат файла: строки "клавиша = команда"; "#" — комментарий; "клавиша = none" снимает привязку.
// Имена клавиш: одиночный символ или Up Down Left Right Enter Esc Space Tab Backspace PageUp PageDown Home End.
class KeyBindings {
 public:
  static KeyBindings withDefaults();

  // Возвращает предупреждения (по одному на плохую строку); сама загрузка не падает.
  std::vector<std::string> loadFile(const std::string& path, const CommandRegistry& commands);
  std::vector<std::string> load(std::istream& in, const CommandRegistry& commands);

  void bind(std::string key, std::string command) { keyToCommand_[std::move(key)] = std::move(command); }
  std::optional<std::string> commandFor(const std::string& key) const;
  std::vector<std::string> keysFor(const std::string& command) const;

 private:
  std::map<std::string, std::string> keyToCommand_;
};

}  // namespace mail
