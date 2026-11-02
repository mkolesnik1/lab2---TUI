#include "core/application/message_renderer.h"

#include <cstdio>

#include "core/domain/rfc2047.h"
#include "core/domain/summary.h"
#include "core/domain/text_util.h"

namespace mail {

std::string sanitizeFilename(std::string_view name) {
  std::string s;
  for (char32_t cp : decodeUtf8(name)) {
    if (cp < 0x20 || cp == 0x7F || cp == '/' || cp == '\\' || cp == ':') cp = '_';
    appendUtf8(s, cp);
  }
  size_t start = 0;
  while (start < s.size() && (s[start] == '.' || s[start] == ' ')) ++start;  // нет ".." и скрытых файлов
  return s.substr(start);
}

namespace {

std::string formatSize(size_t bytes) {
  char buf[32];
  if (bytes < 1024) std::snprintf(buf, sizeof buf, "%zu B", bytes);
  else if (bytes < 1024 * 1024) std::snprintf(buf, sizeof buf, "%.1f KB", static_cast<double>(bytes) / 1024.0);
  else std::snprintf(buf, sizeof buf, "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
  return buf;
}

// Visitor: плоский обход дерева частей с глубиной.
class PartCollector final : public PartVisitor {
 public:
  struct Node {
    const Part* part;
    const LeafPart* leaf;
    const Part* embeddedMessage;  // для message/rfc822 — вложенное письмо
    int depth;
    bool embedded;  // лежит внутри message/rfc822
  };
  std::vector<Node> nodes;

  void visit(const LeafPart& p) override { nodes.push_back({&p, &p, nullptr, depth_, embedded_}); }
  void visit(const MultipartPart& p) override {
    nodes.push_back({&p, nullptr, nullptr, depth_, embedded_});
    ++depth_;
    for (const auto& c : p.children()) c->accept(*this);
    --depth_;
  }
  void visit(const MessagePart& p) override {
    nodes.push_back({&p, nullptr, &p.inner(), depth_, embedded_});
    ++depth_;
    const bool saved = embedded_;
    embedded_ = true;
    p.inner().accept(*this);
    embedded_ = saved;
    --depth_;
  }

 private:
  int depth_ = 0;
  bool embedded_ = false;
};

}  // namespace

RenderedMessage MessageRenderer::render(const Message& msg) const {
  RenderedMessage out;

  for (const char* name : {"From", "To", "Cc", "Date", "Subject"}) {
    auto v = msg.headers().get(name);
    if (!v) continue;
    std::string text = decodeHeaderValue(*v, conv_);
    for (char& c : text)
      if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    out.headers.emplace_back(name, std::string(trim(text)));
  }

  PartCollector collector;
  msg.root().accept(collector);

  const IContentHandler* bestHandler = nullptr;
  const LeafPart* bestLeaf = nullptr;
  auto consider = [&](bool allowEmbedded) {
    for (const auto& n : collector.nodes) {
      if (!n.leaf || n.leaf->isAttachment() || (n.embedded && !allowEmbedded)) continue;
      const IContentHandler* h = handlers_.find(n.leaf->mimeType());
      if (h && (!bestHandler || h->priority() < bestHandler->priority())) {
        bestHandler = h;  // побеждает обработчик с наименьшим priority (plain раньше html)
        bestLeaf = n.leaf;
      }
    }
  };
  consider(false);
  if (!bestLeaf) consider(true);
  out.body = bestLeaf ? bestHandler->render(*bestLeaf, conv_) : "(нет текстового содержимого)";

  for (const auto& n : collector.nodes) {
    PartEntry e;
    e.depth = n.depth;
    e.leaf = n.leaf;
    const std::string mime = n.part->mimeType();
    if (n.leaf) {
      e.attachment = n.leaf->isAttachment();
      e.filename = sanitizeFilename(decodeHeaderValue(n.leaf->rawFilename(), conv_));
      e.label = mime;
      if (!e.filename.empty()) e.label += "  " + e.filename;
      e.label += "  (" + formatSize(n.leaf->decodedBody().size()) + ")";
    } else if (n.embeddedMessage) {
      e.label = mime + ": " + std::string(trim(decodeHeaderValue(n.embeddedMessage->headers().getOr("subject"), conv_)));
    } else {
      e.label = mime;
    }
    out.parts.push_back(std::move(e));
  }
  return out;
}

}  // namespace mail
