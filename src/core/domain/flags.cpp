#include "core/domain/flags.h"

namespace mail {

std::string Flags::toMaildir() const {
  std::string s;  // порядок букв строго по алфавиту: D F R S T
  if (draft) s += 'D';
  if (flagged) s += 'F';
  if (replied) s += 'R';
  if (seen) s += 'S';
  if (trashed) s += 'T';
  return s;
}

Flags Flags::fromMaildir(std::string_view letters) {
  Flags f;
  for (char c : letters) {
    switch (c) {
      case 'D': f.draft = true; break;
      case 'F': f.flagged = true; break;
      case 'R': f.replied = true; break;
      case 'S': f.seen = true; break;
      case 'T': f.trashed = true; break;
      default: break;  // P и неизвестные флаги игнорируем
    }
  }
  return f;
}

}  // namespace mail
