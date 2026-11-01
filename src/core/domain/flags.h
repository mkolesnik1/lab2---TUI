#pragma once
#include <string>
#include <string_view>

namespace mail {

// Идентификатор письма внутри хранилища (для Maildir — уникальное имя файла без флагов).
// Не путать с заголовком Message-ID.
using StoreId = std::string;

// Флаги Maildir. В имени файла: "<id>:2,<буквы по алфавиту>", например ":2,FRS".
struct Flags {
  bool draft = false;    // D
  bool flagged = false;  // F  (помечено)
  bool replied = false;  // R
  bool seen = false;     // S  (прочитано)
  bool trashed = false;  // T

  std::string toMaildir() const;
  static Flags fromMaildir(std::string_view letters);
  bool operator==(const Flags&) const = default;
};

}  // namespace mail
