#pragma once
#include <map>
#include <string>
#include <string_view>

namespace mail {

// Разбор заголовков вида "значение; имя=параметр; имя2=\"параметр\"" (Content-Type, Content-Disposition).
// Поддерживает RFC 2231 (filename*=utf-8''%D0..., filename*0*=...): результат приводится к
// виду RFC 2047 («=?utf-8?Q?=D0..?=»), чтобы дальше работал один декодер.
struct HeaderParams {
  std::string value;                         // в нижнем регистре: "text/plain", "attachment"
  std::map<std::string, std::string> params;  // имена в нижнем регистре

  static HeaderParams parse(std::string_view raw);
  std::string param(const std::string& name) const;  // "" если нет
};

}  // namespace mail
