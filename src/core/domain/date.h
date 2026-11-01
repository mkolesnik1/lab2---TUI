#pragma once
#include <ctime>
#include <string_view>

namespace mail {

// RFC 5322 date ("Tue, 13 Oct 2026 15:21:00 +0700") -> UTC unix time. 0, если разобрать нельзя.
std::time_t parseDate(std::string_view raw);

}  // namespace mail
