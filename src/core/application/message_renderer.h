#pragma once
#include <string>
#include <utility>
#include <vector>

#include "core/application/content_handlers.h"
#include "core/domain/charset.h"
#include "core/domain/mime.h"

namespace mail {

// Строка дерева частей письма для экрана.
struct PartEntry {
  int depth = 0;
  std::string label;                // "text/plain  (1.2 KB)", "message/rfc822: тема"
  std::string filename;             // раскодированное и безопасное имя ("" у контейнеров)
  const LeafPart* leaf = nullptr;   // nullptr у multipart и message/rfc822
  bool attachment = false;
};

struct RenderedMessage {
  std::vector<std::pair<std::string, std::string>> headers;  // From, To, Cc, Date, Subject — в UTF-8
  std::string body;                                           // текст в UTF-8
  std::vector<PartEntry> parts;                               // плоский обход дерева
};

// Имя файла без пути и управляющих символов; пустое -> "".
std::string sanitizeFilename(std::string_view name);

class MessageRenderer {
 public:
  MessageRenderer(const ContentHandlerRegistry& handlers, const ICharsetConverter& conv)
      : handlers_(handlers), conv_(conv) {}
  // Указатели PartEntry::leaf живут, пока жив Message.
  RenderedMessage render(const Message& msg) const;

 private:
  const ContentHandlerRegistry& handlers_;
  const ICharsetConverter& conv_;
};

}  // namespace mail
