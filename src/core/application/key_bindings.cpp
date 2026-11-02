#include "core/application/key_bindings.h"

#include <filesystem>
#include <fstream>

#include "core/domain/text_util.h"

namespace mail {

KeyBindings KeyBindings::withDefaults() {
  KeyBindings k;
  const std::pair<const char*, const char*> defaults[] = {
      {"Down", "down"},       {"Up", "up"},
      {"Space", "page-down"}, {"PageDown", "page-down"}, {"b", "page-up"},    {"PageUp", "page-up"},
      {"g", "top"},           {"Home", "top"},         {"G", "bottom"},       {"End", "bottom"},
      {"Enter", "open"},      {"l", "open"},           {"Right", "open"},     {"q", "back-or-quit"},
      {"Esc", "back"},        {"Left", "back"},        {"h", "back"},         {"Q", "quit"},
      {"?", "help"},          {"F1", "help"},          {"R", "refresh"},      {"f", "toggle-flagged"},
      {"m", "toggle-seen"},   {"1", "sort-date"},      {"2", "sort-sender"},  {"3", "sort-subject"},
      {"v", "reverse-sort"},  {"t", "toggle-threads"}, {"/", "search"},       {"c", "clear-filter"},
      {"p", "toggle-parts"},  {"s", "save-attachment"}, {"w", "save-text"},   {"e", "export-eml"}};
  for (const auto& [key, cmd] : defaults) k.bind(key, cmd);
  return k;
}

std::vector<std::string> KeyBindings::load(std::istream& in, const CommandRegistry& commands) {
  std::vector<std::string> warnings;
  std::string line;
  int lineNo = 0;
  while (std::getline(in, line)) {
    ++lineNo;
    const std::string_view t = trim(line);
    if (t.empty() || t[0] == '#') continue;
    const size_t eq = t.find('=', t[0] == '=' ? 1 : 0);  // сама клавиша может быть "="
    const std::string where = "конфиг клавиш, строка " + std::to_string(lineNo) + ": ";
    if (eq == std::string_view::npos) {
      warnings.push_back(where + "ожидается «клавиша = команда»");
      continue;
    }
    const std::string key(trim(t.substr(0, eq)));
    const std::string cmd(trim(t.substr(eq + 1)));
    if (key.empty() || cmd.empty()) {
      warnings.push_back(where + "пустая клавиша или команда");
    } else if (cmd == "none") {
      keyToCommand_.erase(key);
    } else if (!commands.find(cmd)) {
      warnings.push_back(where + "неизвестная команда «" + cmd + "»");
    } else {
      bind(key, cmd);
    }
  }
  return warnings;
}

std::vector<std::string> KeyBindings::loadFile(const std::string& path, const CommandRegistry& commands) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return {};  // файла нет — значения по умолчанию
  std::ifstream in(path);
  if (!in) return {"не удалось прочитать конфиг клавиш: " + path};
  return load(in, commands);
}

std::optional<std::string> KeyBindings::commandFor(const std::string& key) const {
  auto it = keyToCommand_.find(key);
  if (it == keyToCommand_.end()) return std::nullopt;
  return it->second;
}

std::vector<std::string> KeyBindings::keysFor(const std::string& command) const {
  std::vector<std::string> keys;
  for (const auto& [key, cmd] : keyToCommand_)
    if (cmd == command) keys.push_back(key);
  return keys;
}

}  // namespace mail
