#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "core/domain/mail_store.h"

namespace mail {

// Factory + реестр: создаёт хранилище по ключу ("maildir", потом "mbox", "imap").
// Новое хранилище = новый класс + одна строка регистрации.
class StoreRegistry {
 public:
  using Creator = std::function<std::unique_ptr<IMailStore>(const std::string& location)>;
  void add(std::string type, Creator creator);
  std::unique_ptr<IMailStore> create(const std::string& type, const std::string& location) const;  // StoreError
  std::string knownTypes() const;  // "maildir, mbox"

 private:
  std::map<std::string, Creator> creators_;
};

}  // namespace mail
