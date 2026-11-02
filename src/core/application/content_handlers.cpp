#include "core/application/content_handlers.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <utility>
#include <vector>

#include "core/domain/text_util.h"

namespace mail {

const IContentHandler* ContentHandlerRegistry::find(const std::string& mimeType) const {
  for (const auto& h : handlers_)
    if (h->canHandle(mimeType)) return h.get();
  return nullptr;
}

std::string decodeTextBody(const LeafPart& part, const ICharsetConverter& conv) {
  return safeToUtf8(conv, part.decodedBody(), part.charset());
}

namespace {

size_t findCI(std::string_view hay, std::string_view needle, size_t from) {
  for (size_t i = from; i + needle.size() <= hay.size(); ++i)
    if (iequals(hay.substr(i, needle.size()), needle)) return i;
  return std::string_view::npos;
}

// Пытается разобрать сущность в позиции i (html[i] == '&'). Возвращает true и сдвигает i.
bool decodeEntity(std::string_view html, size_t& i, std::string& out) {
  const size_t semi = html.find(';', i + 1);
  if (semi == std::string_view::npos || semi - i > 10) return false;
  const std::string_view name = html.substr(i + 1, semi - i - 1);
  if (name.empty()) return false;
  if (name[0] == '#') {
    const bool hex = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
    const std::string digits(name.substr(hex ? 2 : 1));
    if (digits.empty()) return false;
    char* end = nullptr;
    const unsigned long cp = std::strtoul(digits.c_str(), &end, hex ? 16 : 10);
    if (*end != '\0' || cp == 0 || cp > 0x10FFFF) return false;
    appendUtf8(out, static_cast<char32_t>(cp));
  } else {
    static const struct {
      const char* name;
      const char* text;
    } kEntities[] = {{"amp", "&"},   {"lt", "<"},       {"gt", ">"},       {"quot", "\""},    {"apos", "'"},
                     {"nbsp", " "},  {"copy", "©"}, {"mdash", "—"}, {"ndash", "–"},
                     {"hellip", "…"}, {"laquo", "«"}, {"raquo", "»"}, {"ldquo", "“"},
                     {"rdquo", "”"}, {"lsquo", "‘"}, {"rsquo", "’"}, {"euro", "€"},
                     {"bull", "•"}};
    bool found = false;
    for (const auto& e : kEntities)
      if (name == e.name) {
        out += e.text;
        found = true;
        break;
      }
    if (!found) return false;
  }
  i = semi + 1;
  return true;
}

void ensureBreak(std::string& out, int newlines) {
  if (out.empty()) return;
  while (!out.empty() && out.back() == ' ') out.pop_back();
  int have = 0;
  for (size_t k = out.size(); k > 0 && out[k - 1] == '\n' && have < 2; --k) ++have;
  for (; have < newlines; ++have) out += '\n';
}

}  // namespace

std::string htmlToText(std::string_view html) {
  std::string out;
  size_t i = 0;
  while (i < html.size()) {
    const char c = html[i];
    if (c == '<') {
      if (html.substr(i, 4) == "<!--") {
        const size_t e = html.find("-->", i + 4);
        i = e == std::string_view::npos ? html.size() : e + 3;
        continue;
      }
      const size_t e = html.find('>', i + 1);
      if (e == std::string_view::npos) {  // одиночный '<'
        out += c;
        ++i;
        continue;
      }
      std::string_view tag = html.substr(i + 1, e - i - 1);
      const bool closing = !tag.empty() && tag[0] == '/';
      if (closing) tag.remove_prefix(1);
      size_t n = 0;
      while (n < tag.size() && std::isalnum(static_cast<unsigned char>(tag[n]))) ++n;
      const std::string name = toLowerAscii(tag.substr(0, n));
      i = e + 1;

      if (!closing && (name == "script" || name == "style" || name == "head")) {  // содержимое пропускаем
        const size_t close = findCI(html, "</" + name, i);
        if (close == std::string_view::npos) {
          i = html.size();
        } else {
          const size_t gt = html.find('>', close);
          i = gt == std::string_view::npos ? html.size() : gt + 1;
        }
      } else if (name == "br") {
        while (!out.empty() && out.back() == ' ') out.pop_back();
        out += '\n';
      } else if (name == "p" || name == "table" || name == "ul" || name == "ol" || name == "blockquote" ||
                 name == "pre" || name == "hr" || (name.size() == 2 && name[0] == 'h' && std::isdigit(static_cast<unsigned char>(name[1])))) {
        ensureBreak(out, 2);
      } else if (name == "div" || name == "tr") {
        ensureBreak(out, 1);
      } else if (name == "li" && !closing) {
        ensureBreak(out, 1);
        out += "• ";
      } else if ((name == "td" || name == "th") && closing) {
        if (!out.empty() && out.back() != ' ' && out.back() != '\n') out += ' ';
      }
      continue;
    }
    if (c == '&' && decodeEntity(html, i, out)) continue;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      if (!out.empty() && out.back() != ' ' && out.back() != '\n') out += ' ';
    } else {
      out += c;
    }
    ++i;
  }
  while (!out.empty() && (out.back() == ' ' || out.back() == '\n')) out.pop_back();
  return out;
}

namespace {

// Значение iCalendar: \n -> перевод строки, \, \; \\ -> сам символ.
std::string icalUnescape(std::string_view v) {
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) {
    if (v[i] == '\\' && i + 1 < v.size()) {
      const char n = v[++i];
      out += (n == 'n' || n == 'N') ? '\n' : n;
    } else {
      out += v[i];
    }
  }
  return out;
}

// 20261105T100000Z -> "2026-11-05 10:00 UTC"; 20261105 -> "2026-11-05"; непонятное — как есть.
std::string icalDate(const std::string& v) {
  auto digits = [&](size_t from, size_t n) {
    if (from + n > v.size()) return false;
    for (size_t k = from; k < from + n; ++k)
      if (!std::isdigit(static_cast<unsigned char>(v[k]))) return false;
    return true;
  };
  if (!digits(0, 8)) return v;
  std::string out = v.substr(0, 4) + "-" + v.substr(4, 2) + "-" + v.substr(6, 2);
  if (v.size() >= 13 && v[8] == 'T' && digits(9, 4)) {
    out += " " + v.substr(9, 2) + ":" + v.substr(11, 2);
    if (v.back() == 'Z') out += " UTC";
  }
  return out;
}

}  // namespace

std::string icalToText(std::string_view ics) {
  // 1) «разворачиваем» строки: продолжение начинается с пробела или табуляции
  std::vector<std::string> lines;
  size_t pos = 0;
  while (pos < ics.size()) {
    size_t end = ics.find('\n', pos);
    if (end == std::string_view::npos) end = ics.size();
    std::string line(ics.substr(pos, end - pos));
    pos = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty() && (line[0] == ' ' || line[0] == '\t') && !lines.empty())
      lines.back() += line.substr(1);
    else
      lines.push_back(std::move(line));
  }

  // 2) собираем свойства каждого VEVENT (берём первое вхождение свойства)
  static const std::pair<const char*, const char*> kFields[] = {
      {"SUMMARY", "Событие"}, {"DTSTART", "Начало"},    {"DTEND", "Конец"},
      {"LOCATION", "Место"},  {"ORGANIZER", "Организатор"}, {"DESCRIPTION", "Описание"}};
  std::vector<std::string> events;
  std::map<std::string, std::string> props;
  bool inEvent = false;
  auto flush = [&] {
    std::string text;
    for (const auto& [key, label] : kFields) {
      auto it = props.find(key);
      if (it == props.end() || it->second.empty()) continue;
      if (!text.empty()) text += '\n';
      text += std::string(label) + ": " + it->second;
    }
    if (!text.empty()) events.push_back(std::move(text));
    props.clear();
  };
  for (const auto& line : lines) {
    if (line == "BEGIN:VEVENT") {
      inEvent = true;
      continue;
    }
    if (line == "END:VEVENT") {
      flush();
      inEvent = false;
      continue;
    }
    if (!inEvent) continue;
    const size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string name = line.substr(0, std::min(colon, line.find(';')));  // без параметров
    for (char& ch : name) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    std::string value = icalUnescape(line.substr(colon + 1));
    if (name == "DTSTART" || name == "DTEND") value = icalDate(value);
    if (name == "ORGANIZER" && value.rfind("mailto:", 0) == 0) value.erase(0, 7);
    props.emplace(std::move(name), std::move(value));
  }

  if (events.empty()) return std::string(ics);  // не календарь — покажем как есть
  std::string out;
  for (const auto& e : events) out += (out.empty() ? "" : "\n\n") + e;
  return out;
}

namespace {

class CalendarHandler final : public IContentHandler {
 public:
  bool canHandle(const std::string& mime) const override { return mime == "text/calendar"; }
  int priority() const override { return 20; }  // после plain (0) и html (10)
  std::string render(const LeafPart& part, const ICharsetConverter& conv) const override {
    return icalToText(decodeTextBody(part, conv));
  }
};

class PlainTextHandler final : public IContentHandler {
 public:
  bool canHandle(const std::string& mime) const override { return mime == "text/plain"; }
  int priority() const override { return 0; }
  std::string render(const LeafPart& part, const ICharsetConverter& conv) const override {
    return decodeTextBody(part, conv);
  }
};

class HtmlTextHandler final : public IContentHandler {
 public:
  bool canHandle(const std::string& mime) const override { return mime == "text/html"; }
  int priority() const override { return 10; }
  std::string render(const LeafPart& part, const ICharsetConverter& conv) const override {
    return htmlToText(decodeTextBody(part, conv));
  }
};

}  // namespace

void registerBuiltinHandlers(ContentHandlerRegistry& r) {
  r.add(std::make_unique<PlainTextHandler>());
  r.add(std::make_unique<HtmlTextHandler>());
  r.add(std::make_unique<CalendarHandler>());
}

}  // namespace mail
