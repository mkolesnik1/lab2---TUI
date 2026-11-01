#include "check.h"
#include "core/domain/header_params.h"
#include "core/domain/rfc2047.h"
#include "infrastructure/iconv_converter.h"

using namespace mail;

namespace {
IconvConverter conv;
std::string dec(const std::string& s) { return decodeHeaderValue(s, conv); }
}  // namespace

TEST(rfc2047, base64_and_q_words) {
  CHECK_EQ(dec("=?utf-8?B?0J/RgNC40LLQtdGC?="), std::string("Привет"));
  CHECK_EQ(dec("=?koi8-r?Q?=F0=D2=C9=D7=C5=D4_=CD=C9=D2?="), std::string("Привет мир"));
  CHECK_EQ(dec("=?ISO-8859-1?q?Caf=E9?="), std::string("Café"));
}

TEST(rfc2047, mixed_with_plain_text) {
  CHECK_EQ(dec("Re: =?utf-8?B?0J/RgNC40LLQtdGC?= (draft)"), std::string("Re: Привет (draft)"));
  CHECK_EQ(dec("plain subject"), std::string("plain subject"));
}

TEST(rfc2047, whitespace_between_adjacent_words_is_dropped) {
  CHECK_EQ(dec("=?utf-8?B?0J/RgNC4?= =?utf-8?B?0LLQtdGC?="), std::string("Привет"));
  CHECK_EQ(dec("=?utf-8?B?0J/RgNC4?=\n =?utf-8?B?0LLQtdGC?="), std::string("Привет"));
}

TEST(rfc2047, multibyte_char_split_between_words) {
  // «Привет»: 12 байт UTF-8, разрезаны посреди символа (3 + 9)
  CHECK_EQ(dec("=?utf-8?B?0J/R?= =?utf-8?B?gNC40LLQtdGC?="), std::string("Привет"));
}

TEST(rfc2047, broken_words_stay_literal) {
  CHECK_EQ(dec("=?utf-8?X?abc?="), std::string("=?utf-8?X?abc?="));
  CHECK_EQ(dec("=?utf-8?B?abc"), std::string("=?utf-8?B?abc"));
  CHECK_EQ(dec("=?"), std::string("=?"));
}

TEST(rfc2047, language_suffix_and_unknown_charset) {
  CHECK_EQ(dec("=?utf-8*ru?B?0J/RgNC40LLQtdGC?="), std::string("Привет"));
  CHECK_EQ(dec("=?x-unknown?Q?caf=E9?="), std::string("café"));  // запасной Latin-1
}

TEST(rfc2047, header_params_basic_and_quoted) {
  const auto p = HeaderParams::parse("Multipart/Mixed; Boundary=\"a;b\"; charset=UTF-8");
  CHECK_EQ(p.value, std::string("multipart/mixed"));
  CHECK_EQ(p.param("boundary"), std::string("a;b"));
  CHECK_EQ(p.param("charset"), std::string("UTF-8"));
  CHECK_EQ(p.param("nothing"), std::string(""));
}

TEST(rfc2047, header_params_rfc2231) {
  const auto a = HeaderParams::parse("attachment; filename*=utf-8''%D0%B0%D0%B1.pdf");
  CHECK_EQ(dec(a.param("filename")), std::string("аб.pdf"));
  const auto b = HeaderParams::parse(
      "attachment; filename*0*=utf-8''%D0%B0;\n filename*1*=%D0%B1.pdf");
  CHECK_EQ(dec(b.param("filename")), std::string("аб.pdf"));
  const auto c = HeaderParams::parse("attachment; filename*0=\"long\"; filename*1=\"name.txt\"");
  CHECK_EQ(c.param("filename"), std::string("longname.txt"));
}
