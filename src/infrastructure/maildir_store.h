#pragma once
#include <filesystem>
#include <string>

#include "core/domain/mail_store.h"

namespace mail {

// Maildir: <root>/new, <root>/cur. Имя файла "<id>:2,<флаги>". Смена флагов — атомарный rename.
// Хранилище не держит состояния, поэтому безопасно для вызова из фонового потока.
class MaildirStore final : public IMailStore {
 public:
  explicit MaildirStore(std::filesystem::path root);  // StoreError, если нет new/ и cur/

  std::string name() const override { return root_.string(); }
  std::vector<StoredMessage> list() const override;
  std::string readRaw(const StoreId& id) const override;
  void setFlags(const StoreId& id, Flags flags) override;

 private:
  std::filesystem::path findPath(const StoreId& id) const;  // StoreError, если файла нет
  std::filesystem::path root_;
};

}  // namespace mail
