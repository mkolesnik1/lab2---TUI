#include "core/domain/summary.h"

#include "core/domain/date.h"
#include "core/domain/rfc2047.h"
#include "core/domain/text_util.h"

namespace mail {
namespace {

// Заголовки могут содержать переводы строк/табы после раскодирования — сводим к одному пробелу.
std::string collapseSpaces(std::string_view s) {
  std::string out;
  for (char c : s) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      if (!out.empty() && out.back() != ' ') out += ' ';
    } else {
      out += c;
    }
  }
  return std::string(trim(out));
}

}  // namespace

std::vector<std::string> extractMessageIds(std::string_view value) {
  std::vector<std::string> ids;
  size_t pos = 0;
  while (true) {
    const size_t a = value.find('<', pos);
    if (a == std::string_view::npos) break;
    const size_t b = value.find('>', a + 1);
    if (b == std::string_view::npos) break;
    const std::string_view id = trim(value.substr(a + 1, b - a - 1));
    if (!id.empty()) ids.emplace_back(id);
    pos = b + 1;
  }
  if (ids.empty() && !trim(value).empty() && value.find('<') == std::string_view::npos)
    ids.emplace_back(trim(value));  // Message-ID без угловых скобок
  return ids;
}

MessageSummary makeSummary(const StoreId& id, Flags flags, const Headers& h, const ICharsetConverter& conv) {
  MessageSummary s;
  s.id = id;
  s.flags = flags;
  if (auto ids = extractMessageIds(h.getOr("message-id")); !ids.empty()) s.messageId = ids.front();
  if (auto ids = extractMessageIds(h.getOr("in-reply-to")); !ids.empty()) s.inReplyTo = ids.front();
  s.references = extractMessageIds(h.getOr("references"));
  s.from = collapseSpaces(decodeHeaderValue(h.getOr("from"), conv));
  s.subject = collapseSpaces(decodeHeaderValue(h.getOr("subject"), conv));
  s.date = parseDate(h.getOr("date"));
  return s;
}

std::string shortSender(const std::string& from) {
  const size_t lt = from.find('<');
  const size_t gt = lt == std::string::npos ? lt : from.find('>', lt);
  if (lt == std::string::npos || gt == std::string::npos) return from;
  std::string name(trim(std::string_view(from).substr(0, lt)));
  if (name.size() >= 2 && name.front() == '"' && name.back() == '"') name = name.substr(1, name.size() - 2);
  if (name.empty()) name = std::string(trim(std::string_view(from).substr(lt + 1, gt - lt - 1)));
  return name;
}

}  // namespace mail
