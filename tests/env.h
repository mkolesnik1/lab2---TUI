#pragma once
// Зависимости ядра, собранные так же, как в app/main.cpp (нужны тестам сеанса, рендера и расширений).
#include "core/application/commands.h"
#include "core/application/content_handlers.h"
#include "core/application/session.h"
#include "core/domain/filters.h"
#include "core/domain/sorting.h"
#include "infrastructure/iconv_converter.h"
#include "support.h"

namespace testutil {

// Все зависимости ядра, собранные так же, как в app/main.cpp.
struct Env {
  mail::IconvConverter conv;
  mail::ContentHandlerRegistry handlers;
  mail::SortRegistry sorts;
  mail::FilterRegistry filters;
  mail::CommandRegistry commands;
  Env() {
    mail::registerBuiltinHandlers(handlers);
    mail::registerBuiltinSorts(sorts);
    mail::registerBuiltinFilters(filters);
    mail::registerBuiltinCommands(commands);
  }
  mail::MailboxSession::Services services() { return {handlers, sorts, filters}; }
};

// Ставит курсор списка на письмо с данным id.
inline bool selectById(mail::MailboxSession& s, const std::string& id) {
  s.moveToEdge(true);
  for (size_t i = 0; i < s.rows().size(); ++i) {
    if (s.selected() && s.selected()->id == id) return true;
    s.move(1);
  }
  return false;
}

}  // namespace testutil
