#include "core/domain/sorting.h"

#include "core/domain/text_util.h"

namespace mail {
namespace {

// Тема без префиксов "Re:", "Fwd:" — чтобы ответы стояли рядом с исходным письмом.
std::string baseSubject(const std::string& subject) {
  std::string s = lowerUtf8(subject);
  while (true) {
    const std::string_view t = trim(s);
    size_t cut = 0;
    for (std::string_view p : {"re:", "fwd:", "fw:", "re :"})
      if (t.substr(0, p.size()) == p) cut = p.size();
    if (cut == 0) return std::string(t);
    s = std::string(t.substr(cut));
  }
}

class DateSort final : public ISortStrategy {
 public:
  std::string name() const override { return "date"; }
  bool less(const MessageSummary& a, const MessageSummary& b) const override {
    return a.date != b.date ? a.date < b.date : a.id < b.id;
  }
  bool defaultDescending() const override { return true; }  // новые сверху
};

class SenderSort final : public ISortStrategy {
 public:
  std::string name() const override { return "sender"; }
  bool less(const MessageSummary& a, const MessageSummary& b) const override {
    const std::string x = lowerUtf8(shortSender(a.from)), y = lowerUtf8(shortSender(b.from));
    return x != y ? x < y : a.id < b.id;
  }
  bool defaultDescending() const override { return false; }
};

class SubjectSort final : public ISortStrategy {
 public:
  std::string name() const override { return "subject"; }
  bool less(const MessageSummary& a, const MessageSummary& b) const override {
    const std::string x = baseSubject(a.subject), y = baseSubject(b.subject);
    return x != y ? x < y : a.id < b.id;
  }
  bool defaultDescending() const override { return false; }
};

}  // namespace

void SortRegistry::add(std::unique_ptr<ISortStrategy> s) {
  const std::string key = s->name();
  items_[key] = std::move(s);
}

const ISortStrategy* SortRegistry::find(const std::string& name) const {
  auto it = items_.find(name);
  return it == items_.end() ? nullptr : it->second.get();
}

void registerBuiltinSorts(SortRegistry& r) {
  r.add(std::make_unique<DateSort>());
  r.add(std::make_unique<SenderSort>());
  r.add(std::make_unique<SubjectSort>());
}

}  // namespace mail
