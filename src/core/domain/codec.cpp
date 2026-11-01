#include "core/domain/codec.h"

#include "core/domain/text_util.h"

namespace mail {
namespace {

int base64Value(unsigned char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

}  // namespace

// Терпимый к мусору base64: пропускает символы вне алфавита, останавливается на '='.
std::string Base64Decoder::decode(std::string_view in) const {
  std::string out;
  out.reserve(in.size() / 4 * 3);
  unsigned buf = 0;
  int bits = 0;
  for (char ch : in) {
    if (ch == '=') break;
    const int v = base64Value(static_cast<unsigned char>(ch));
    if (v < 0) continue;
    buf = ((buf << 6) | static_cast<unsigned>(v)) & 0xFFFFF;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += static_cast<char>((buf >> bits) & 0xFF);
    }
  }
  return out;
}

// Quoted-printable: =XX, мягкий перенос "=\n". Некорректное "=" оставляем как есть.
std::string QuotedPrintableDecoder::decode(std::string_view in) const {
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size(); ++i) {
    if (in[i] != '=') {
      out += in[i];
      continue;
    }
    if (i + 1 < in.size() && in[i + 1] == '\n') {  // мягкий перенос
      ++i;
      continue;
    }
    if (i + 2 < in.size() && in[i + 1] == '\r' && in[i + 2] == '\n') {
      i += 2;
      continue;
    }
    if (i + 2 < in.size() && hexValue(in[i + 1]) >= 0 && hexValue(in[i + 2]) >= 0) {
      out += static_cast<char>(hexValue(in[i + 1]) * 16 + hexValue(in[i + 2]));
      i += 2;
      continue;
    }
    out += '=';
  }
  return out;
}

std::unique_ptr<IDecoder> DecoderFactory::forEncoding(std::string_view cte) {
  const std::string e = toLowerAscii(trim(cte));
  if (e == "base64") return std::make_unique<Base64Decoder>();
  if (e == "quoted-printable") return std::make_unique<QuotedPrintableDecoder>();
  return std::make_unique<IdentityDecoder>();  // 7bit, 8bit, binary, пусто, неизвестное
}

}  // namespace mail
