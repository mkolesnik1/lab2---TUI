#include "core/domain/mime.h"

#include "core/domain/codec.h"
#include "core/domain/errors.h"
#include "core/domain/header_params.h"
#include "core/domain/text_util.h"

namespace mail {

// ---- Part ---------------------------------------------------------------

std::string Part::mimeType() const {
  const std::string v = HeaderParams::parse(headers_.getOr("content-type")).value;
  return v.empty() ? "text/plain" : v;
}

std::string Part::charset() const { return HeaderParams::parse(headers_.getOr("content-type")).param("charset"); }

std::string Part::encoding() const {
  const std::string v = toLowerAscii(trim(headers_.getOr("content-transfer-encoding")));
  return v.empty() ? "7bit" : v;
}

std::string Part::disposition() const { return HeaderParams::parse(headers_.getOr("content-disposition")).value; }

std::string Part::rawFilename() const {
  std::string name = HeaderParams::parse(headers_.getOr("content-disposition")).param("filename");
  if (name.empty()) name = HeaderParams::parse(headers_.getOr("content-type")).param("name");
  return name;
}

bool Part::isAttachment() const {
  const std::string d = disposition();
  return d == "attachment" || (d != "inline" && !rawFilename().empty());
}

std::string LeafPart::decodedBody() const { return DecoderFactory::forEncoding(encoding())->decode(body_); }

// ---- Parser ---------------------------------------------------------------

namespace {

constexpr int kMaxDepth = 24;  // защита от бесконечной вложенности

struct HeadBody {
  std::string_view head, body;
};

HeadBody splitHeadBody(std::string_view t) {
  if (!t.empty() && t[0] == '\n') return {{}, t.substr(1)};  // письмо без заголовков
  const size_t p = t.find("\n\n");
  if (p == std::string_view::npos) return {t, {}};
  return {t.substr(0, p + 1), t.substr(p + 2)};
}

std::vector<std::string_view> splitMultipart(std::string_view body, const std::string& boundary) {
  const std::string delim = "--" + boundary;
  const std::string close = delim + "--";
  std::vector<std::string_view> parts;
  bool inPart = false;
  size_t partStart = 0, pos = 0;
  while (pos <= body.size()) {
    const size_t eol = body.find('\n', pos);
    const size_t lineEnd = eol == std::string_view::npos ? body.size() : eol;
    std::string_view line = body.substr(pos, lineEnd - pos);
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.remove_suffix(1);
    const bool isClose = line == close;
    const bool isDelim = line == delim;
    if (isDelim || isClose) {
      if (inPart) parts.push_back(body.substr(partStart, (pos > partStart ? pos - 1 : partStart) - partStart));
      inPart = isDelim;
      partStart = eol == std::string_view::npos ? body.size() : eol + 1;
      if (isClose) break;
    }
    if (eol == std::string_view::npos) break;
    pos = eol + 1;
  }
  if (inPart && partStart <= body.size()) parts.push_back(body.substr(partStart));  // нет закрывающей границы
  return parts;
}

std::unique_ptr<Part> parseEntity(std::string_view text, int depth) {
  const auto [head, body] = splitHeadBody(text);
  Headers h = Headers::parse(head);
  const HeaderParams ct = HeaderParams::parse(h.getOr("content-type"));
  const std::string mime = ct.value.empty() ? "text/plain" : ct.value;

  if (depth < kMaxDepth) {
    if (istartsWith(mime, "multipart/")) {
      const std::string boundary = ct.param("boundary");
      if (!boundary.empty()) {  // без boundary — считаем обычным листом
        auto mp = std::make_unique<MultipartPart>(std::move(h));
        for (std::string_view piece : splitMultipart(body, boundary)) mp->add(parseEntity(piece, depth + 1));
        return mp;
      }
    } else if (mime == "message/rfc822") {
      const std::string inner = DecoderFactory::forEncoding(h.getOr("content-transfer-encoding"))->decode(body);
      return std::make_unique<MessagePart>(std::move(h), parseEntity(inner, depth + 1));
    }
  }
  return std::make_unique<LeafPart>(std::move(h), std::string(body));
}

}  // namespace

Message MimeParser::parse(std::string_view raw) {
  if (trim(raw).empty()) throw ParseError("пустое письмо");
  const std::string text = normalizeNewlines(raw);
  return Message(parseEntity(text, 0));
}

Headers MimeParser::parseHeaders(std::string_view raw) {
  if (trim(raw).empty()) throw ParseError("пустое письмо");
  const std::string text = normalizeNewlines(raw.substr(0, 65536));  // заголовки не бывают больше
  return Headers::parse(splitHeadBody(text).head);
}

}  // namespace mail
