#include "core/domain/text_util.h"

namespace mail {

std::string toLowerAscii(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
  return out;
}

std::string_view trim(std::string_view s) {
  const char* ws = " \t\r\n";
  const size_t b = s.find_first_not_of(ws);
  if (b == std::string_view::npos) return {};
  const size_t e = s.find_last_not_of(ws);
  return s.substr(b, e - b + 1);
}

bool iequals(std::string_view a, std::string_view b) {
  return a.size() == b.size() && toLowerAscii(a) == toLowerAscii(b);
}

bool istartsWith(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && iequals(s.substr(0, prefix.size()), prefix);
}

std::string normalizeNewlines(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\r') {
      out += '\n';
      if (i + 1 < s.size() && s[i + 1] == '\n') ++i;
    } else {
      out += s[i];
    }
  }
  return out;
}

std::vector<char32_t> decodeUtf8(std::string_view s) {
  std::vector<char32_t> out;
  size_t i = 0;
  while (i < s.size()) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    const int len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
    if (len == 0 || i + static_cast<size_t>(len) > s.size()) {
      out.push_back(0xFFFD);
      ++i;
      continue;
    }
    char32_t cp = len == 1 ? c : static_cast<char32_t>(c & (0xFF >> (len + 1)));
    bool ok = true;
    for (int k = 1; k < len; ++k) {
      const unsigned char cc = static_cast<unsigned char>(s[i + static_cast<size_t>(k)]);
      if ((cc & 0xC0) != 0x80) {
        ok = false;
        break;
      }
      cp = (cp << 6) | (cc & 0x3F);
    }
    if (!ok) {
      out.push_back(0xFFFD);
      ++i;
      continue;
    }
    out.push_back(cp);
    i += static_cast<size_t>(len);
  }
  return out;
}

void appendUtf8(std::string& out, char32_t cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

bool isValidUtf8(std::string_view s) {
  std::string back;
  for (char32_t cp : decodeUtf8(s)) appendUtf8(back, cp);
  return back == s;  // битые байты и «overlong» дают расхождение
}

std::string latin1ToUtf8(std::string_view s) {
  std::string out;
  for (char c : s) appendUtf8(out, static_cast<unsigned char>(c));
  return out;
}

namespace {
char32_t lowerCp(char32_t c) {
  if (c >= 'A' && c <= 'Z') return c + 32;
  if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
  if (c >= 0x410 && c <= 0x42F) return c + 32;  // А-Я
  if (c >= 0x400 && c <= 0x40F) return c + 80;  // Ѐ-Џ (в т.ч. Ё)
  if ((c >= 0x100 && c <= 0x137 && c % 2 == 0) || (c >= 0x14A && c <= 0x177 && c % 2 == 0) ||
      (c >= 0x139 && c <= 0x148 && c % 2 == 1) || (c >= 0x179 && c <= 0x17E && c % 2 == 1))
    return c + 1;  // Latin Extended-A
  return c;
}
}  // namespace

std::string lowerUtf8(std::string_view s) {
  std::string out;
  for (char32_t cp : decodeUtf8(s)) appendUtf8(out, lowerCp(cp));
  return out;
}

int cellWidth(char32_t cp) {
  if (cp >= 0x300 && cp <= 0x36F) return 0;  // комбинирующие
  if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) || (cp >= 0xAC00 && cp <= 0xD7A3) ||
      (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
      (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x20000 && cp <= 0x3FFFD))
    return 2;  // CJK и «широкие» символы
  return 1;
}

std::string fitToWidth(std::string_view s, int cols, bool pad) {
  if (cols <= 0) return {};
  std::vector<char32_t> cps = decodeUtf8(s);
  int total = 0;
  for (char32_t& cp : cps) {
    if (cp < 0x20 || cp == 0x7F) cp = ' ';  // табы и управляющие символы
    total += cellWidth(cp);
  }
  const bool truncate = total > cols;
  const int limit = truncate ? cols - 1 : cols;
  std::string out;
  int used = 0;
  for (char32_t cp : cps) {
    const int w = cellWidth(cp);
    if (used + w > limit) break;
    appendUtf8(out, cp);
    used += w;
  }
  if (truncate) {
    appendUtf8(out, 0x2026);  // …
    ++used;
  }
  if (pad && used < cols) out.append(static_cast<size_t>(cols - used), ' ');
  return out;
}

namespace {
void wrapLine(std::string_view line, int cols, std::vector<std::string>& out) {
  std::vector<char32_t> cps;
  for (char32_t cp : decodeUtf8(line)) {
    if (cp == '\t') cps.push_back(' ');
    else if (cp >= 0x20 && cp != 0x7F) cps.push_back(cp);
  }
  if (cps.empty()) {
    out.emplace_back();
    return;
  }
  std::string cur;
  int curW = 0;
  auto flush = [&] {
    out.push_back(cur);
    cur.clear();
    curW = 0;
  };
  size_t i = 0;
  while (i < cps.size()) {
    size_t j = i;
    while (j < cps.size() && cps[j] == ' ') ++j;  // пробелы перед словом
    size_t k = j;
    while (k < cps.size() && cps[k] != ' ') ++k;  // слово
    int spaces = static_cast<int>(j - i);
    int wordW = 0;
    for (size_t m = j; m < k; ++m) wordW += cellWidth(cps[m]);
    if (curW > 0 && curW + spaces + wordW > cols) {
      flush();
      spaces = 0;  // пробелы на месте переноса отбрасываем
    }
    cur.append(static_cast<size_t>(spaces), ' ');
    curW += spaces;
    for (size_t m = j; m < k; ++m) {
      const int w = cellWidth(cps[m]);
      if (curW + w > cols && curW > 0) flush();
      appendUtf8(cur, cps[m]);
      curW += w;
    }
    i = k;
  }
  if (!cur.empty()) flush();
}
}  // namespace

std::vector<std::string> wrapText(std::string_view text, int cols) {
  if (cols < 1) cols = 1;
  std::vector<std::string> lines;
  size_t pos = 0;
  while (true) {
    const size_t nl = text.find('\n', pos);
    wrapLine(text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos), cols, lines);
    if (nl == std::string_view::npos) break;
    pos = nl + 1;
  }
  return lines;
}

}  // namespace mail
