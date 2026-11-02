#pragma once
#include "core/application/store_registry.h"

namespace mail {

// Регистрация хранилищ. Чтобы добавить mbox: написать MboxStore : IMailStore
// и добавить одну строку r.add("mbox", ...) в builtin_stores.cpp.
void registerBuiltinStores(StoreRegistry& r);

}  // namespace mail
