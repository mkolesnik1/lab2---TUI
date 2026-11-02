#include "core/application/store_registry.h"

#include "core/domain/errors.h"

namespace mail {

void StoreRegistry::add(std::string type, Creator creator) { creators_[std::move(type)] = std::move(creator); }

std::unique_ptr<IMailStore> StoreRegistry::create(const std::string& type, const std::string& location) const {
  auto it = creators_.find(type);
  if (it == creators_.end()) throw StoreError("неизвестный тип хранилища: " + type + " (есть: " + knownTypes() + ")");
  return it->second(location);
}

std::string StoreRegistry::knownTypes() const {
  std::string s;
  for (const auto& [name, creator] : creators_) {
    (void)creator;
    if (!s.empty()) s += ", ";
    s += name;
  }
  return s;
}

}  // namespace mail
