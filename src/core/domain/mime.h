#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/domain/headers.h"

namespace mail {

class LeafPart;
class MultipartPart;
class MessagePart;

// Visitor: разные правила обхода для листа, multipart и вложенного письма
// без цепочек dynamic_cast.
class PartVisitor {
 public:
  virtual ~PartVisitor() = default;
  virtual void visit(const LeafPart& part) = 0;
  virtual void visit(const MultipartPart& part) = 0;
  virtual void visit(const MessagePart& part) = 0;
};

// Composite: дерево частей письма.
class Part {
 public:
  virtual ~Part() = default;
  Part(const Part&) = delete;
  Part& operator=(const Part&) = delete;

  virtual void accept(PartVisitor& v) const = 0;

  const Headers& headers() const { return headers_; }
  std::string mimeType() const;       // "text/plain" по умолчанию
  std::string charset() const;        // "" если не указан
  std::string encoding() const;       // Content-Transfer-Encoding, "7bit" по умолчанию
  std::string disposition() const;    // "attachment" / "inline" / ""
  std::string rawFilename() const;    // как в письме (может быть в RFC 2047)
  bool isAttachment() const;

 protected:
  explicit Part(Headers h) : headers_(std::move(h)) {}

 private:
  Headers headers_;
};

// Лист: text/plain, image/png, application/pdf ...
class LeafPart final : public Part {
 public:
  LeafPart(Headers h, std::string body) : Part(std::move(h)), body_(std::move(body)) {}
  void accept(PartVisitor& v) const override { v.visit(*this); }
  const std::string& rawBody() const { return body_; }
  std::string decodedBody() const;  // шаг 1: снята кодировка передачи (base64 / QP / ...)

 private:
  std::string body_;
};

class MultipartPart final : public Part {
 public:
  explicit MultipartPart(Headers h) : Part(std::move(h)) {}
  void accept(PartVisitor& v) const override { v.visit(*this); }
  void add(std::unique_ptr<Part> child) { children_.push_back(std::move(child)); }
  const std::vector<std::unique_ptr<Part>>& children() const { return children_; }

 private:
  std::vector<std::unique_ptr<Part>> children_;
};

// message/rfc822: внутри целое письмо (корень нового дерева).
class MessagePart final : public Part {
 public:
  MessagePart(Headers h, std::unique_ptr<Part> inner) : Part(std::move(h)), inner_(std::move(inner)) {}
  void accept(PartVisitor& v) const override { v.visit(*this); }
  const Part& inner() const { return *inner_; }

 private:
  std::unique_ptr<Part> inner_;
};

// Письмо = корневая часть (её заголовки — заголовки письма).
class Message {
 public:
  explicit Message(std::unique_ptr<Part> root) : root_(std::move(root)) {}
  const Part& root() const { return *root_; }
  const Headers& headers() const { return root_->headers(); }

 private:
  std::unique_ptr<Part> root_;
};

class MimeParser {
 public:
  // Бросает ParseError только для пустого письма; всё остальное разбирается «как получится».
  static Message parse(std::string_view raw);
  // Только заголовки (быстро, для списка писем).
  static Headers parseHeaders(std::string_view raw);
};

}  // namespace mail
