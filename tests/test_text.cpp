#include "check.h"
#include "core/domain/text_util.h"

using namespace mail;

TEST(text, lower_utf8_handles_cyrillic_and_latin) {
  CHECK_EQ(lowerUtf8("ПРИВЕТ Ёж"), std::string("привет ёж"));
  CHECK_EQ(lowerUtf8("Café ÉCOLE"), std::string("café école"));
  CHECK_EQ(lowerUtf8("ŁÓDŹ"), std::string("łódź"));
}

TEST(text, utf8_validation_and_latin1_fallback) {
  CHECK(isValidUtf8("Привет"));
  CHECK(!isValidUtf8("caf\xe9"));
  CHECK_EQ(latin1ToUtf8("caf\xe9"), std::string("café"));
  CHECK_EQ(decodeUtf8("a\xff").size(), size_t{2});  // битый байт -> U+FFFD, а не падение
}

TEST(text, fit_to_width_truncates_pads_and_counts_wide_chars) {
  CHECK_EQ(fitToWidth("abc", 5, true), std::string("abc  "));
  CHECK_EQ(fitToWidth("abcdef", 4, false), std::string("abc…"));
  CHECK_EQ(fitToWidth("привет", 4, true), std::string("при…"));
  CHECK_EQ(fitToWidth("日本語", 4, true), std::string("日… "));  // иероглиф = 2 ячейки, остаток добит пробелом
  CHECK_EQ(fitToWidth("a\tb", 5, true), std::string("a b  "));  // табы -> пробелы
}

TEST(text, wrap_text_wraps_words_and_hard_breaks_long_words) {
  const auto lines = wrapText("hello world foo", 11);
  CHECK_EQ(lines.size(), size_t{2});
  CHECK_EQ(lines[0], std::string("hello world"));
  CHECK_EQ(lines[1], std::string("foo"));

  const auto hard = wrapText("abcdefghij", 4);
  CHECK_EQ(hard.size(), size_t{3});
  CHECK_EQ(hard[2], std::string("ij"));

  const auto blank = wrapText("a\n\nb", 10);  // пустые строки сохраняются
  CHECK_EQ(blank.size(), size_t{3});
  CHECK_EQ(blank[1], std::string(""));
}
