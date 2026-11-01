#include "infrastructure/iconv_converter.h"

#include <iconv.h>

#include <cerrno>

#include "core/domain/errors.h"

namespace mail {
namespace {

class IconvHandle {  // RAII: iconv_close в деструкторе
 public:
  explicit IconvHandle(const std::string& from) : cd_(iconv_open("UTF-8", from.c_str())) {
    if (cd_ == reinterpret_cast<iconv_t>(-1)) throw DecodeError("неизвестная кодировка: " + from);
  }
  ~IconvHandle() { iconv_close(cd_); }
  IconvHandle(const IconvHandle&) = delete;
  IconvHandle& operator=(const IconvHandle&) = delete;
  iconv_t get() const { return cd_; }

 private:
  iconv_t cd_;
};

}  // namespace

std::string IconvConverter::toUtf8(std::string_view bytes, std::string_view charset) const {
  IconvHandle handle{std::string(charset)};
  std::string src(bytes);
  std::string out;
  char* in = src.data();
  size_t left = src.size();
  char buf[4096];
  while (left > 0) {
    char* o = buf;
    size_t oleft = sizeof buf;
    const size_t r = iconv(handle.get(), &in, &left, &o, &oleft);
    out.append(buf, static_cast<size_t>(o - buf));
    if (r == static_cast<size_t>(-1) && errno != E2BIG) {  // EILSEQ / EINVAL: непереводимый байт
      out += "\xEF\xBF\xBD";                                 // U+FFFD
      ++in;
      --left;
    }
  }
  return out;
}

}  // namespace mail
