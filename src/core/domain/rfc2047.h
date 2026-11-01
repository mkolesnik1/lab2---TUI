#pragma once
#include <string>
#include <string_view>

#include "core/domain/charset.h"

namespace mail {

// Раскодирует encoded-words вида =?charset?B|Q?text?= в UTF-8.
// Пробелы между соседними encoded-words отбрасываются; слова с одним charset склеиваются
// до перекодировки (многобайтный символ может быть разрезан между словами).
std::string decodeHeaderValue(std::string_view raw, const ICharsetConverter& conv);

}  // namespace mail
