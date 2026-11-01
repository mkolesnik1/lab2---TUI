#pragma once
#include "core/domain/charset.h"

namespace mail {

// Перекодировка через iconv (glibc). Дескриптор открывается на каждый вызов (RAII), поэтому
// класс без состояния и потокобезопасен.
class IconvConverter final : public ICharsetConverter {
 public:
  std::string toUtf8(std::string_view bytes, std::string_view charset) const override;
};

}  // namespace mail
