#include <algorithm>
#include <string>
#include <vector>

#include "check.h"
#include "core/domain/sorting.h"
#include "core/domain/threading.h"

using namespace mail;

namespace {

MessageSummary msg(const std::string& id, long date, std::vector<std::string> refs = {}, const std::string& reply = "") {
  MessageSummary m;
  m.id = id;
  m.messageId = id;
  m.date = date;
  m.references = std::move(refs);
  m.inReplyTo = reply;
  m.subject = id;
  return m;
}

std::vector<ThreadRow> rowsOf(const std::vector<MessageSummary>& v, bool descending = false) {
  SortRegistry sorts;
  registerBuiltinSorts(sorts);
  return ThreadBuilder::flatten(ThreadBuilder::build(v, *sorts.find("date"), descending));
}

std::string shape(const std::vector<MessageSummary>& v, const std::vector<ThreadRow>& rows) {
  std::string s;
  for (const ThreadRow& r : rows) {
    s += std::string(static_cast<size_t>(r.depth), '.');
    s += r.index < 0 ? "?" : v[static_cast<size_t>(r.index)].id;
    s += " ";
  }
  return s;
}

}  // namespace

TEST(threads, chain_and_siblings_form_a_tree) {
  std::vector<MessageSummary> v = {msg("root", 1), msg("b", 2, {"root"}, "root"), msg("c", 3, {"root", "b"}, "b"),
                                   msg("d", 4, {"root"}, "root")};
  CHECK_EQ(shape(v, rowsOf(v)), std::string("root .b ..c .d "));
}

TEST(threads, input_order_does_not_matter) {
  std::vector<MessageSummary> v = {msg("d", 4, {"root"}, "root"), msg("c", 3, {"root", "b"}, "b"), msg("root", 1),
                                   msg("b", 2, {"root"}, "root")};
  CHECK_EQ(shape(v, rowsOf(v)), std::string("root .b ..c .d "));
}

TEST(threads, in_reply_to_alone_is_enough) {
  std::vector<MessageSummary> v = {msg("a", 1), msg("b", 2, {}, "a")};
  CHECK_EQ(shape(v, rowsOf(v)), std::string("a .b "));
}

TEST(threads, missing_parent_gets_placeholder_node) {
  std::vector<MessageSummary> v = {msg("orphan", 5, {"gone"}, "gone"), msg("other", 1)};
  CHECK_EQ(shape(v, rowsOf(v)), std::string("other ? .orphan "));
}

TEST(threads, missing_middle_of_references_chain_keeps_grouping) {
  std::vector<MessageSummary> v = {msg("a", 1), msg("c", 3, {"a", "b-missing"}, "b-missing")};
  CHECK_EQ(shape(v, rowsOf(v)), std::string("a .? ..c "));
}

TEST(threads, cycles_do_not_hang_and_every_message_appears_once) {
  std::vector<MessageSummary> v = {msg("x", 1, {}, "y"), msg("y", 2, {}, "x"), msg("self", 3, {"self"}, "self")};
  const auto rows = rowsOf(v);
  int seen[3] = {0, 0, 0};
  for (const ThreadRow& r : rows)
    if (r.index >= 0) ++seen[r.index];
  CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1);
}

TEST(threads, descending_roots_newest_first_children_oldest_first) {
  std::vector<MessageSummary> v = {msg("old", 1), msg("old-reply", 2, {"old"}, "old"), msg("new", 10)};
  CHECK_EQ(shape(v, rowsOf(v, true)), std::string("new old .old-reply "));
}

TEST(threads, messages_without_ids_are_independent_roots) {
  std::vector<MessageSummary> v = {msg("a", 1), msg("b", 2)};
  v[0].messageId.clear();
  v[1].messageId.clear();
  CHECK_EQ(rowsOf(v).size(), size_t{2});
}

TEST(threads, sorting_strategies_are_independent_objects) {
  SortRegistry sorts;
  registerBuiltinSorts(sorts);
  MessageSummary a = msg("a", 1), b = msg("b", 2);
  a.from = "Zed <z@x>";
  b.from = "alice <a@x>";
  a.subject = "Re: Beta";
  b.subject = "alpha";
  CHECK(sorts.find("date")->less(a, b));
  CHECK(sorts.find("sender")->less(b, a));    // alice < Zed без учёта регистра
  CHECK(!sorts.find("subject")->less(a, b));  // "Re:" отбрасывается: beta > alpha
  CHECK(sorts.find("nonsense") == nullptr);
}
