#include <string>
#include <vector>

#include "check.h"
#include "core/domain/errors.h"
#include "core/domain/mime.h"

using namespace mail;

namespace {

// Visitor, записывающий структуру дерева: "multipart/mixed(text/plain,message/rfc822(...))".
class Dump final : public PartVisitor {
 public:
  std::string out;
  void visit(const LeafPart& p) override { out += p.mimeType(); }
  void visit(const MultipartPart& p) override {
    out += p.mimeType() + "(";
    bool first = true;
    for (const auto& c : p.children()) {
      if (!first) out += ",";
      first = false;
      c->accept(*this);
    }
    out += ")";
  }
  void visit(const MessagePart& p) override {
    out += p.mimeType() + "[";
    p.inner().accept(*this);
    out += "]";
  }
};

std::string shape(const std::string& raw) {
  Dump d;
  MimeParser::parse(raw).root().accept(d);
  return d.out;
}

}  // namespace

TEST(mime, simple_message_defaults_to_text_plain) {
  const Message m = MimeParser::parse("Subject: hi\n\nbody\n");
  CHECK_EQ(m.root().mimeType(), std::string("text/plain"));
  CHECK_EQ(m.headers().getOr("SUBJECT"), std::string("hi"));  // имена регистронезависимы
}

TEST(mime, folded_headers_are_unfolded) {
  const Message m = MimeParser::parse("Subject: one\n two\n\tthree\nFrom: a@b\n\nx");
  CHECK_EQ(m.headers().getOr("subject"), std::string("one two three"));
}

TEST(mime, nested_multipart) {
  const std::string raw =
      "Content-Type: multipart/mixed; boundary=\"A\"\n\n"
      "--A\nContent-Type: multipart/alternative; boundary=\"B\"\n\n"
      "--B\nContent-Type: text/plain\n\nplain\n"
      "--B\nContent-Type: text/html\n\n<p>html</p>\n"
      "--B--\n"
      "--A\nContent-Type: application/pdf\n\nPDF\n"
      "--A--\n";
  CHECK_EQ(shape(raw), std::string("multipart/mixed(multipart/alternative(text/plain,text/html),application/pdf)"));
}

TEST(mime, embedded_rfc822_message) {
  const std::string raw =
      "Content-Type: multipart/mixed; boundary=X\n\n"
      "--X\nContent-Type: text/plain\n\nnote\n"
      "--X\nContent-Type: message/rfc822\n\n"
      "Subject: inner\nContent-Type: multipart/alternative; boundary=Y\n\n"
      "--Y\nContent-Type: text/plain\n\nin\n--Y--\n"
      "--X--\n";
  CHECK_EQ(shape(raw), std::string("multipart/mixed(text/plain,message/rfc822[multipart/alternative(text/plain)])"));
}

TEST(mime, part_bodies_are_exact) {
  const Message m = MimeParser::parse(
      "Content-Type: multipart/mixed; boundary=B\n\n--B\nContent-Type: text/plain\n\nline1\nline2\n--B--\n");
  const auto& mp = static_cast<const MultipartPart&>(m.root());  // тест знает форму письма
  const auto& leaf = static_cast<const LeafPart&>(*mp.children().at(0));
  CHECK_EQ(leaf.rawBody(), std::string("line1\nline2"));
}

TEST(mime, transfer_decoding_of_leaf) {
  const Message m = MimeParser::parse("Content-Transfer-Encoding: base64\n\nSGVsbG8=\n");
  CHECK_EQ(static_cast<const LeafPart&>(m.root()).decodedBody(), std::string("Hello"));
}

TEST(mime, crlf_line_endings) {
  const std::string raw =
      "Content-Type: multipart/mixed; boundary=B\r\n\r\n--B\r\nContent-Type: text/plain\r\n\r\nhi\r\n--B--\r\n";
  CHECK_EQ(shape(raw), std::string("multipart/mixed(text/plain)"));
}

TEST(mime, broken_input_is_tolerated) {
  CHECK_EQ(shape("Content-Type: multipart/mixed\n\ntext"), std::string("multipart/mixed"));  // нет boundary
  CHECK_EQ(shape("Content-Type: multipart/mixed; boundary=B\n\n--B\nContent-Type: text/plain\n\nx\n"),
           std::string("multipart/mixed(text/plain)"));  // нет закрывающей границы
  CHECK_EQ(shape("no headers at all"), std::string("text/plain"));
  CHECK_EQ(shape("\nbody without headers"), std::string("text/plain"));
}

TEST(mime, empty_message_throws_parse_error) {
  CHECK_THROWS(MimeParser::parse(""), ParseError);
  CHECK_THROWS(MimeParser::parse(" \n\n"), ParseError);
  CHECK_THROWS(MimeParser::parseHeaders(""), ParseError);
}

TEST(mime, excessive_nesting_does_not_overflow_stack) {
  std::string raw;
  for (int i = 0; i < 200; ++i) raw += "Content-Type: multipart/mixed; boundary=B" + std::to_string(i) + "\n\n--B" + std::to_string(i) + "\n";
  raw += "Content-Type: text/plain\n\nx\n";
  CHECK_EQ(MimeParser::parse(raw).root().mimeType(), std::string("multipart/mixed"));  // не упали
}

TEST(mime, attachment_detection) {
  const Message a = MimeParser::parse("Content-Disposition: attachment; filename=a.txt\nContent-Type: text/plain\n\nx");
  CHECK(a.root().isAttachment());
  CHECK_EQ(a.root().rawFilename(), std::string("a.txt"));
  const Message b = MimeParser::parse("Content-Type: text/plain\n\nx");
  CHECK(!b.root().isAttachment());
  const Message c = MimeParser::parse("Content-Type: image/png; name=pic.png\n\nx");
  CHECK(c.root().isAttachment());  // есть имя и это не inline
}
