#include "check.h"
#include "core/domain/codec.h"

using namespace mail;

TEST(codec, base64_rfc4648_vectors) {
  Base64Decoder d;
  CHECK_EQ(d.decode(""), std::string(""));
  CHECK_EQ(d.decode("Zg=="), std::string("f"));
  CHECK_EQ(d.decode("Zm8="), std::string("fo"));
  CHECK_EQ(d.decode("Zm9v"), std::string("foo"));
  CHECK_EQ(d.decode("Zm9vYg=="), std::string("foob"));
  CHECK_EQ(d.decode("Zm9vYmE="), std::string("fooba"));
  CHECK_EQ(d.decode("Zm9vYmFy"), std::string("foobar"));
}

TEST(codec, base64_ignores_line_breaks_and_garbage) {
  Base64Decoder d;
  CHECK_EQ(d.decode("Zm9v\nYmFy\n"), std::string("foobar"));
  CHECK_EQ(d.decode("SGVsbG8s IHdv@@cmxkIQ==\n!!!"), std::string("Hello, world!"));
  CHECK_EQ(d.decode("Zm9vYg"), std::string("foob"));  // без padding
}

TEST(codec, base64_binary_bytes) {
  const std::string out = Base64Decoder().decode("AAECA/8=");
  CHECK_EQ(out.size(), size_t{5});
  CHECK_EQ(static_cast<unsigned char>(out[4]), 0xFFu);
}

TEST(codec, quoted_printable_basics) {
  QuotedPrintableDecoder d;
  CHECK_EQ(d.decode("caf=C3=A9"), std::string("caf\xC3\xA9"));
  CHECK_EQ(d.decode("a=3Db"), std::string("a=b"));
  CHECK_EQ(d.decode("soft=\nbreak"), std::string("softbreak"));
  CHECK_EQ(d.decode("soft=\r\nbreak"), std::string("softbreak"));
  CHECK_EQ(d.decode("lower=c3=a9"), std::string("lower\xC3\xA9"));
}

TEST(codec, quoted_printable_tolerates_garbage) {
  QuotedPrintableDecoder d;
  CHECK_EQ(d.decode("100=ZZ%"), std::string("100=ZZ%"));
  CHECK_EQ(d.decode("end="), std::string("end="));
}

TEST(codec, factory_selects_decoder_by_header_value) {
  CHECK_EQ(DecoderFactory::forEncoding("BASE64")->decode("Zm9v"), std::string("foo"));
  CHECK_EQ(DecoderFactory::forEncoding(" quoted-printable ")->decode("=41"), std::string("A"));
  CHECK_EQ(DecoderFactory::forEncoding("7bit")->decode("=41"), std::string("=41"));
  CHECK_EQ(DecoderFactory::forEncoding("")->decode("x"), std::string("x"));
  CHECK_EQ(DecoderFactory::forEncoding("x-unknown")->decode("x"), std::string("x"));
}
