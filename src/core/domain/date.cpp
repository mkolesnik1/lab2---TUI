#include "core/domain/date.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

#include "core/domain/text_util.h"

namespace mail {

std::time_t parseDate(std::string_view raw) {
  std::string s(raw);
  if (const size_t p = s.find('('); p != std::string::npos) s.erase(p);  // "(UTC)"
  if (const size_t c = s.find(','); c != std::string::npos) s.erase(0, c + 1);

  std::istringstream in(s);
  int day = 0, year = 0;
  std::string mon, hms, zone;
  if (!(in >> day >> mon >> year >> hms)) return 0;
  in >> zone;

  static const char* const kMonths[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                        "jul", "aug", "sep", "oct", "nov", "dec"};
  int m = -1;
  const std::string lm = toLowerAscii(mon).substr(0, 3);
  for (int i = 0; i < 12; ++i)
    if (lm == kMonths[i]) m = i;
  if (m < 0 || day < 1 || day > 31) return 0;
  if (year < 100) year += year < 50 ? 2000 : 1900;

  int hh = 0, mm = 0, ss = 0;
  if (std::sscanf(hms.c_str(), "%d:%d:%d", &hh, &mm, &ss) < 2) return 0;

  std::tm t{};
  t.tm_year = year - 1900;
  t.tm_mon = m;
  t.tm_mday = day;
  t.tm_hour = hh;
  t.tm_min = mm;
  t.tm_sec = ss;
  std::time_t result = timegm(&t);

  if (zone.size() >= 5 && (zone[0] == '+' || zone[0] == '-')) {
    const int v = std::atoi(zone.c_str() + 1);
    int off = (v / 100) * 3600 + (v % 100) * 60;
    if (zone[0] == '-') off = -off;
    result -= off;
  }
  return result;
}

}  // namespace mail
