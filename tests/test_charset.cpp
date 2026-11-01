#include "check.h"
#include "core/domain/charset.h"
#include "core/domain/errors.h"
#include "infrastructure/iconv_converter.h"

using namespace mail;

TEST(charset, iconv_converts_legacy_encodings) {
  IconvConverter c;
  CHECK_EQ(c.toUtf8("\xf0\xd2\xc9\xd7\xc5\xd4", "koi8-r"), std::string("Привет"));
  CHECK_EQ(c.toUtf8("\xcf\xf0\xe8\xe2\xe5\xf2", "windows-1251"), std::string("Привет"));
  CHECK_EQ(c.toUtf8("caf\xe9", "iso-8859-1"), std::string("café"));
  CHECK_EQ(c.toUtf8("\x82\xb1\x82\xf1\x82\xc9\x82\xbf\x82\xcd", "shift_jis"), std::string("こんにちは"));
}

TEST(charset, unknown_charset_throws_decode_error) {
  IconvConverter c;
  CHECK_THROWS(c.toUtf8("abc", "x-no-such-charset"), DecodeError);
}

TEST(charset, invalid_bytes_become_replacement_char) {
  IconvConverter c;
  CHECK_EQ(c.toUtf8("a\xff" "b", "utf-8"), std::string("a\xEF\xBF\xBD" "b"));
}

TEST(charset, safe_conversion_never_throws_and_falls_back_to_latin1) {
  IconvConverter c;
  CHECK_EQ(safeToUtf8(c, "caf\xe9", "x-no-such-charset"), std::string("café"));
  CHECK_EQ(safeToUtf8(c, "Привет", "us-ascii"), std::string("Привет"));  // «us-ascii», но в письме UTF-8
  CHECK_EQ(safeToUtf8(c, "caf\xe9", ""), std::string("café"));
  CHECK_EQ(safeToUtf8(c, "\xf0\xd2\xc9\xd7\xc5\xd4", "KOI8-R"), std::string("Привет"));
}
