#pragma once
#include <vector>

#include "core/domain/sorting.h"
#include "core/domain/summary.h"

namespace mail {

// Composite: узел дерева треда. index == -1 — «заглушка» для письма, которого нет в ящике,
// но на которое ссылаются другие.
struct ThreadNode {
  int index = -1;  // индекс в массиве summaries
  std::vector<ThreadNode> children;
};

struct ThreadRow {
  int index;  // -1 для заглушки
  int depth;
};

// Группировка по Message-ID / In-Reply-To / References (упрощённый алгоритм JWZ).
class ThreadBuilder {
 public:
  // Корни сортируются по выбранному критерию, дети — по дате (старые сверху).
  static std::vector<ThreadNode> build(const std::vector<MessageSummary>& msgs, const ISortStrategy& rootOrder,
                                       bool descending);
  static std::vector<ThreadRow> flatten(const std::vector<ThreadNode>& roots);
};

}  // namespace mail
