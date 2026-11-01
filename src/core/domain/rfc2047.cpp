#include "core/domain/rfc2047.h"

#include <optional>

#include "core/domain/codec.h"
#include "core/domain/text_util.h"

namespace mail {
namespace {

struct Word {
  std::string charset;
  std::string bytes;
  size_t end = 0;  // позиция сразу после "?="
};

std::optional<Word> parseWord(std::string_view s, size_t start) {
  const size_t csEnd = s.find('?', start + 2);
  if (csEnd == std::string_view::npos || csEnd + 2 >= s.size() || s[csEnd + 2] != '?') return std::nullopt;
  const char enc = s[csEnd + 1];
  const size_t textEnd = s.find("?=", csEnd + 3);
  if (textEnd == std::string_view::npos) return std::nullopt;

  Word w;
  w.charset = std::string(s.substr(start + 2, csEnd - start - 2));
  if (const size_t star = w.charset.find('*'); star != std::string::npos) w.charset.erase(star);  // RFC 2231: язык
  w.end = textEnd + 2;
  const std::string_view text = s.substr(csEnd + 3, textEnd - csEnd - 3);
  if (enc == 'B' || enc == 'b') {
    w.bytes = Base64Decoder().decode(text);
  } else if (enc == 'Q' || enc == 'q') {
    std::string t(text);
    for (char& c : t)
      if (c == '_') c = ' ';  // в Q-кодировке '_' означает пробел
    w.bytes = QuotedPrintableDecoder().decode(t);
  } else {
    return std::nullopt;
  }
  return w;
}

bool allSpace(std::string_view s) { return trim(s).empty(); }

}  // namespace

std::string decodeHeaderValue(std::string_view s, const ICharsetConverter& conv) {
  std::string out, pending, pendingCharset;
  auto flush = [&] {
    if (!pending.empty()) out += safeToUtf8(conv, pending, pendingCharset);
    pending.clear();
  };
  bool lastWasWord = false;
  size_t i = 0;
  while (i < s.size()) {
    const size_t start = s.find("=?", i);
    if (start == std::string_view::npos) {
      flush();
      out.append(s.substr(i));
      break;
    }
    const std::string_view literal = s.substr(i, start - i);
    if (auto w = parseWord(s, start)) {
      if (!(lastWasWord && allSpace(literal))) {  // пробелы между encoded-words не показываем
        flush();
        out.append(literal);
      }
      if (pendingCharset != w->charset) flush();
      pendingCharset = w->charset;
      pending += w->bytes;
      lastWasWord = true;
      i = w->end;
    } else {  // "=?" без корректного слова — обычный текст
      flush();
      out.append(literal);
      out.append("=?");
      lastWasWord = false;
      i = start + 2;
    }
  }
  flush();
  return out;
}

}  // namespace mail
