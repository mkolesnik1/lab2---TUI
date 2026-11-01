#pragma once
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

#include "core/domain/charset.h"
#include "core/domain/flags.h"
#include "core/domain/headers.h"

namespace mail {

// Краткие сведения о письме для списка, тредов, сортировки и поиска.
// Все строки уже раскодированы (RFC 2047 -> UTF-8).
struct MessageSummary {
  StoreId id;
  Flags flags;
  std::string messageId;   // без <>
  std::string inReplyTo;
  std::vector<std::string> references;
  std::string from;        // "Имя <addr>" целиком
  std::string subject;
  std::time_t date = 0;
};

std::vector<std::string> extractMessageIds(std::string_view value);
MessageSummary makeSummary(const StoreId& id, Flags flags, const Headers& headers, const ICharsetConverter& conv);
std::string shortSender(const std::string& from);  // имя, а если его нет — адрес

}  // namespace mail
