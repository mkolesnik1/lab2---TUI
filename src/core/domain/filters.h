#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "core/domain/summary.h"

namespace mail {

// Strategy: критерий отбора писем. Новый критерий = новый класс + регистрация префикса.
class IFilter {
 public:
  virtual ~IFilter() = default;
  virtual bool matches(const MessageSummary& m) const = 0;
};

// Строка запроса: "from:иван", "subject:отчёт", "is:unread", а без префикса — отправитель ИЛИ тема.
class FilterRegistry {
 public:
  using Factory = std::function<std::unique_ptr<IFilter>(const std::string& argument)>;
  void add(std::string prefix, Factory f);  // prefix "" — фильтр по умолчанию
  std::unique_ptr<IFilter> create(const std::string& query) const;  // nullptr для пустого запроса

 private:
  std::map<std::string, Factory> factories_;
};

void registerBuiltinFilters(FilterRegistry& r);  // from, subject, is, и по умолчанию

}  // namespace mail
