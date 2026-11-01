#include "core/domain/header_params.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "core/domain/text_util.h"

namespace mail {
namespace {

std::vector<std::string_view> splitParams(std::string_view s) {
  std::vector<std::string_view> out;
  size_t start = 0;
  bool quoted = false;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && quoted) ++i;
    else if (s[i] == '"') quoted = !quoted;
    else if (s[i] == ';' && !quoted) {
      out.push_back(s.substr(start, i - start));
      start = i + 1;
    }
  }
  out.push_back(s.substr(start));
  return out;
}

std::string unquote(std::string_view v) {
  v = trim(v);
  if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
    std::string out;
    for (size_t i = 1; i + 1 < v.size(); ++i) {
      if (v[i] == '\\' && i + 2 < v.size()) ++i;
      out += v[i];
    }
    return out;
  }
  return std::string(v);
}

// "utf-8'ru'%D0%B0" -> "=?utf-8?Q?=D0=B0?="
std::string rfc2231ToEncodedWord(const std::string& charset, const std::string& pct) {
  std::string q;
  for (char c : pct) {
    if (c == '%') q += '=';
    else if (c == '_') q += "=5F";
    else q += c;
  }
  return "=?" + charset + "?Q?" + q + "?=";
}

struct Segment {
  std::string text;
  bool encoded = false;
};

}  // namespace

HeaderParams HeaderParams::parse(std::string_view raw) {
  HeaderParams hp;
  const auto parts = splitParams(raw);
  hp.value = toLowerAscii(trim(parts.empty() ? std::string_view{} : parts[0]));
  std::map<std::string, std::map<int, Segment>> continued;  // имя -> номер сегмента -> текст

  for (size_t i = 1; i < parts.size(); ++i) {
    const size_t eq = parts[i].find('=');
    if (eq == std::string_view::npos) continue;
    std::string name = toLowerAscii(trim(parts[i].substr(0, eq)));
    std::string value = unquote(parts[i].substr(eq + 1));
    if (name.empty()) continue;

    bool encoded = false;
    if (name.back() == '*') {
      encoded = true;
      name.pop_back();
    }
    // filename*0 / filename*1 — продолжения
    const size_t star = name.find('*');
    if (star != std::string::npos) {
      const std::string num = name.substr(star + 1);
      if (!num.empty() && std::all_of(num.begin(), num.end(), [](unsigned char c) { return std::isdigit(c); })) {
        continued[name.substr(0, star)][std::stoi(num)] = Segment{value, encoded};
        continue;
      }
    }
    if (encoded) {
      const size_t q1 = value.find('\'');
      const size_t q2 = q1 == std::string::npos ? q1 : value.find('\'', q1 + 1);
      if (q2 != std::string::npos) value = rfc2231ToEncodedWord(value.substr(0, q1), value.substr(q2 + 1));
    }
    hp.params[name] = value;
  }

  for (auto& [name, segs] : continued) {
    std::string charset, data;
    bool firstEncoded = false;
    for (auto& [idx, seg] : segs) {
      std::string text = seg.text;
      if (idx == segs.begin()->first && seg.encoded) {
        const size_t q1 = text.find('\'');
        const size_t q2 = q1 == std::string::npos ? q1 : text.find('\'', q1 + 1);
        if (q2 != std::string::npos) {
          charset = text.substr(0, q1);
          text = text.substr(q2 + 1);
          firstEncoded = true;
        }
      }
      data += text;
    }
    hp.params[name] = firstEncoded ? rfc2231ToEncodedWord(charset, data) : data;
  }
  return hp;
}

std::string HeaderParams::param(const std::string& name) const {
  auto it = params.find(name);
  return it == params.end() ? std::string() : it->second;
}

}  // namespace mail
