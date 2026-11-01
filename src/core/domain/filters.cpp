#include "core/domain/filters.h"

#include "core/domain/text_util.h"

namespace mail {
namespace {

bool containsCI(const std::string& haystack, const std::string& lowerNeedle) {
  return lowerUtf8(haystack).find(lowerNeedle) != std::string::npos;
}

class SenderFilter final : public IFilter {
 public:
  explicit SenderFilter(const std::string& needle) : needle_(lowerUtf8(trim(needle))) {}
  bool matches(const MessageSummary& m) const override { return containsCI(m.from, needle_); }

 private:
  std::string needle_;
};

class SubjectFilter final : public IFilter {
 public:
  explicit SubjectFilter(const std::string& needle) : needle_(lowerUtf8(trim(needle))) {}
  bool matches(const MessageSummary& m) const override { return containsCI(m.subject, needle_); }

 private:
  std::string needle_;
};

class SenderOrSubjectFilter final : public IFilter {
 public:
  explicit SenderOrSubjectFilter(const std::string& needle) : sender_(needle), subject_(needle) {}
  bool matches(const MessageSummary& m) const override { return sender_.matches(m) || subject_.matches(m); }

 private:
  SenderFilter sender_;
  SubjectFilter subject_;
};

// is:unread / is:flagged — пример нового критерия без правки остальных классов.
class StateFilter final : public IFilter {
 public:
  explicit StateFilter(const std::string& state) : state_(toLowerAscii(trim(state))) {}
  bool matches(const MessageSummary& m) const override {
    if (state_ == "unread") return !m.flags.seen;
    if (state_ == "flagged") return m.flags.flagged;
    return false;
  }

 private:
  std::string state_;
};

}  // namespace

void FilterRegistry::add(std::string prefix, Factory f) { factories_[std::move(prefix)] = std::move(f); }

std::unique_ptr<IFilter> FilterRegistry::create(const std::string& query) const {
  const std::string q(trim(query));
  if (q.empty()) return nullptr;
  if (const size_t colon = q.find(':'); colon != std::string::npos) {
    auto it = factories_.find(toLowerAscii(q.substr(0, colon)));
    if (it != factories_.end() && !it->first.empty()) return it->second(q.substr(colon + 1));
  }
  auto def = factories_.find("");
  return def == factories_.end() ? nullptr : def->second(q);
}

void registerBuiltinFilters(FilterRegistry& r) {
  r.add("from", [](const std::string& a) { return std::make_unique<SenderFilter>(a); });
  r.add("subject", [](const std::string& a) { return std::make_unique<SubjectFilter>(a); });
  r.add("is", [](const std::string& a) { return std::make_unique<StateFilter>(a); });
  r.add("", [](const std::string& a) { return std::make_unique<SenderOrSubjectFilter>(a); });
}

}  // namespace mail
