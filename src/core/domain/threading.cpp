#include "core/domain/threading.h"

#include <algorithm>
#include <functional>
#include <unordered_map>

namespace mail {

std::vector<ThreadNode> ThreadBuilder::build(const std::vector<MessageSummary>& msgs, const ISortStrategy& rootOrder,
                                             bool descending) {
  const int n = static_cast<int>(msgs.size());
  std::unordered_map<std::string, int> byId;  // Message-ID -> номер узла
  for (int i = 0; i < n; ++i)
    if (!msgs[static_cast<size_t>(i)].messageId.empty()) byId.emplace(msgs[static_cast<size_t>(i)].messageId, i);

  std::vector<int> parent(static_cast<size_t>(n), -1);  // узлы n.. — заглушки
  auto nodeFor = [&](const std::string& id) {
    if (auto it = byId.find(id); it != byId.end()) return it->second;
    const int k = static_cast<int>(parent.size());
    parent.push_back(-1);
    byId.emplace(id, k);
    return k;
  };
  // Привязать child к par, если у child ещё нет родителя и не получится цикл.
  auto link = [&](int child, int par) {
    if (child == par || parent[static_cast<size_t>(child)] != -1) return;
    for (int a = par; a != -1; a = parent[static_cast<size_t>(a)])
      if (a == child) return;  // par — потомок child: связь создала бы цикл
    parent[static_cast<size_t>(child)] = par;
  };

  for (int i = 0; i < n; ++i) {
    const MessageSummary& m = msgs[static_cast<size_t>(i)];
    std::vector<std::string> chain = m.references;
    if (chain.empty() && !m.inReplyTo.empty()) chain.push_back(m.inReplyTo);
    int prev = -1;
    for (const std::string& id : chain) {
      const int cur = nodeFor(id);
      if (prev != -1) link(cur, prev);  // References: каждый — ответ на предыдущий
      prev = cur;
    }
    if (prev != -1) link(i, prev);
  }

  const int total = static_cast<int>(parent.size());
  std::vector<std::vector<int>> kids(static_cast<size_t>(total));
  std::vector<int> roots;
  for (int i = 0; i < total; ++i) {
    if (parent[static_cast<size_t>(i)] == -1) roots.push_back(i);
    else kids[static_cast<size_t>(parent[static_cast<size_t>(i)])].push_back(i);
  }

  // Письмо, по которому сортируется узел: своё, а для заглушки — самое раннее из потомков.
  std::function<int(int)> rep = [&](int id) {
    if (id < n) return id;
    int best = -1;
    for (int k : kids[static_cast<size_t>(id)]) {
      const int r = rep(k);
      if (r >= 0 && (best < 0 || msgs[static_cast<size_t>(r)].date < msgs[static_cast<size_t>(best)].date)) best = r;
    }
    return best;
  };
  auto byDate = [&](int a, int b) {
    const int ra = rep(a), rb = rep(b);
    if (ra < 0 || rb < 0) return ra > rb;
    const auto& x = msgs[static_cast<size_t>(ra)];
    const auto& y = msgs[static_cast<size_t>(rb)];
    return x.date != y.date ? x.date < y.date : x.id < y.id;
  };

  std::function<ThreadNode(int)> make = [&](int id) {
    ThreadNode t;
    t.index = id < n ? id : -1;
    std::vector<int> ks = kids[static_cast<size_t>(id)];
    std::stable_sort(ks.begin(), ks.end(), byDate);
    for (int k : ks) t.children.push_back(make(k));
    return t;
  };

  std::stable_sort(roots.begin(), roots.end(), [&](int a, int b) {
    const int ra = rep(a), rb = rep(b);
    if (ra < 0 || rb < 0) return ra > rb;
    const auto& x = msgs[static_cast<size_t>(ra)];
    const auto& y = msgs[static_cast<size_t>(rb)];
    return descending ? rootOrder.less(y, x) : rootOrder.less(x, y);
  });

  std::vector<ThreadNode> result;
  for (int r : roots) result.push_back(make(r));
  return result;
}

std::vector<ThreadRow> ThreadBuilder::flatten(const std::vector<ThreadNode>& roots) {
  std::vector<ThreadRow> rows;
  std::function<void(const ThreadNode&, int)> walk = [&](const ThreadNode& node, int depth) {
    rows.push_back({node.index, depth});
    for (const ThreadNode& c : node.children) walk(c, depth + 1);
  };
  for (const ThreadNode& r : roots) walk(r, 0);
  return rows;
}

}  // namespace mail
