#pragma once
#include <stdexcept>

namespace mail {

// Единая иерархия ошибок предметной области. UI ловит MailError и показывает what().
struct MailError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct StoreError : MailError {  // хранилище / индекс недоступны
  using MailError::MailError;
};
struct ParseError : MailError {  // письмо разобрать нельзя совсем
  using MailError::MailError;
};
struct DecodeError : MailError {  // неизвестный charset и т.п.
  using MailError::MailError;
};

}  // namespace mail
