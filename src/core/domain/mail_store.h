#pragma once
#include <string>
#include <vector>

#include "core/domain/flags.h"

namespace mail {

struct StoredMessage {
  StoreId id;
  Flags flags;
};

// Порт хранилища. Maildir, mbox, IMAP (только чтение) — отдельные реализации.
// Остальной код видит только сырые байты письма и флаги, поэтому обход одинаков для любого хранилища.
class IMailStore {
 public:
  virtual ~IMailStore() = default;
  virtual std::string name() const = 0;
  virtual std::vector<StoredMessage> list() const = 0;
  virtual std::string readRaw(const StoreId& id) const = 0;
  // Хранилище только для чтения (IMAP) бросает StoreError.
  virtual void setFlags(const StoreId& id, Flags flags) = 0;
};

}  // namespace mail
