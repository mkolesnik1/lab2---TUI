#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/domain/summary.h"

namespace mail {

// Strategy: критерий сортировки. Новый критерий = новый класс + регистрация.
class ISortStrategy {
 public:
  virtual ~ISortStrategy() = default;
  virtual std::string name() const = 0;   // ключ: "date", "sender", "subject"
  virtual bool less(const MessageSummary& a, const MessageSummary& b) const = 0;  // по возрастанию
  virtual bool defaultDescending() const = 0;
};

class SortRegistry {
 public:
  void add(std::unique_ptr<ISortStrategy> s);
  const ISortStrategy* find(const std::string& name) const;  // nullptr если нет

 private:
  std::map<std::string, std::unique_ptr<ISortStrategy>> items_;
};

void registerBuiltinSorts(SortRegistry& r);  // date, sender, subject

}  // namespace mail
