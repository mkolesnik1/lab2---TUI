#include "core/domain/charset.h"

#include "core/domain/errors.h"
#include "core/domain/text_util.h"

namespace mail {

std::string safeToUtf8(const ICharsetConverter& conv, std::string_view bytes, std::string_view charset) {
  const std::string cs = toLowerAscii(trim(charset));
  if (cs.empty() || cs == "us-ascii" || cs == "ascii" || cs == "utf-8" || cs == "utf8")
    return isValidUtf8(bytes) ? std::string(bytes) : latin1ToUtf8(bytes);
  try {
    return conv.toUtf8(bytes, cs);
  } catch (const DecodeError&) {
    return isValidUtf8(bytes) ? std::string(bytes) : latin1ToUtf8(bytes);
  }
}

}  // namespace mail
