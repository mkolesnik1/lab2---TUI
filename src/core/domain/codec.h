#pragma once
#include <memory>
#include <string>
#include <string_view>

namespace mail {

// Шаг 1 цепочки декодирования: снять кодировку передачи (Content-Transfer-Encoding).
// Шаг 2 (charset -> UTF-8) — см. charset.h.
class IDecoder {
 public:
  virtual ~IDecoder() = default;
  virtual std::string decode(std::string_view in) const = 0;
};

class Base64Decoder final : public IDecoder {
 public:
  std::string decode(std::string_view in) const override;
};

class QuotedPrintableDecoder final : public IDecoder {
 public:
  std::string decode(std::string_view in) const override;
};

class IdentityDecoder final : public IDecoder {  // 7bit / 8bit / binary
 public:
  std::string decode(std::string_view in) const override { return std::string(in); }
};

// Фабрика: выбирает декодер по значению заголовка Content-Transfer-Encoding.
class DecoderFactory {
 public:
  static std::unique_ptr<IDecoder> forEncoding(std::string_view contentTransferEncoding);
};

}  // namespace mail
