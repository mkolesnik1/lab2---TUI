#include "core/domain/headers.h"

#include "core/domain/text_util.h"

namespace mail {

void Headers::add(std::string name, std::string value) { items_.emplace_back(std::move(name), std::move(value)); }

std::optional<std::string> Headers::get(std::string_view name) const {
  for (const auto& [n, v] : items_)
    if (iequals(n, name)) return v;
  return std::nullopt;
}

std::string Headers::getOr(std::string_view name, std::string fallback) const {
  auto v = get(name);
  return v ? *v : std::move(fallback);
}

Headers Headers::parse(std::string_view block) {
  Headers h;
  size_t pos = 0;
  while (pos < block.size()) {
    size_t nl = block.find('\n', pos);
    if (nl == std::string_view::npos) nl = block.size();
    const std::string_view line = block.substr(pos, nl - pos);
    pos = nl + 1;
    if (line.empty()) continue;
    if (line[0] == ' ' || line[0] == '\t') {  // продолжение предыдущего заголовка
      if (!h.items_.empty()) {
        h.items_.back().second += ' ';
        h.items_.back().second += trim(line);
      }
      continue;
    }
    const size_t colon = line.find(':');
    if (colon == std::string_view::npos || colon == 0) continue;
    const std::string_view name = line.substr(0, colon);
    if (name.find_first_of(" \t") != std::string_view::npos) continue;  // например, mbox-строка "From ..."
    h.add(std::string(name), std::string(trim(line.substr(colon + 1))));
  }
  return h;
}

}  // namespace mail
