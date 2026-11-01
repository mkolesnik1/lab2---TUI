#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mail {

// Заголовки письма: имена регистронезависимы, «сложенные» (folded) строки склеены.
// Значения хранятся как в письме (RFC 2047 здесь НЕ раскодируется).
class Headers {
 public:
  void add(std::string name, std::string value);
  std::optional<std::string> get(std::string_view name) const;
  std::string getOr(std::string_view name, std::string fallback = {}) const;
  const std::vector<std::pair<std::string, std::string>>& all() const { return items_; }

  // block — текст заголовков с переводами строк LF. Битые строки пропускаются.
  static Headers parse(std::string_view block);

 private:
  std::vector<std::pair<std::string, std::string>> items_;
};

}  // namespace mail
