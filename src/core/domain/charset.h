#pragma once
#include <string>
#include <string_view>

namespace mail {

// Шаг 2 цепочки декодирования. Реализация (iconv) живёт в infrastructure — ядро iconv не знает.
class ICharsetConverter {
 public:
  virtual ~ICharsetConverter() = default;
  // Бросает DecodeError, если charset неизвестен. Непереводимые байты заменяет на U+FFFD.
  virtual std::string toUtf8(std::string_view bytes, std::string_view charset) const = 0;
};

// «Не падаем на плохой почте»: пустой/us-ascii/utf-8 — оставляем как есть, если это валидный UTF-8,
// иначе читаем как Latin-1; неизвестный charset — тоже Latin-1.
std::string safeToUtf8(const ICharsetConverter& conv, std::string_view bytes, std::string_view charset);

}  // namespace mail
