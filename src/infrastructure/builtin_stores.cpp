#include "infrastructure/builtin_stores.h"

#include "infrastructure/maildir_store.h"

namespace mail {

void registerBuiltinStores(StoreRegistry& r) {
  r.add("maildir", [](const std::string& location) { return std::make_unique<MaildirStore>(location); });
}

}  // namespace mail
