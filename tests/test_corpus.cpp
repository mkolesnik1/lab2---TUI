#include <fstream>
#include <set>
#include <sstream>

#include "check.h"
#include "core/application/message_renderer.h"
#include "core/domain/mime.h"
#include "core/domain/summary.h"
#include "env.h"

using namespace mail;
using namespace testutil;

namespace {

struct Expect {
  std::string id, subject, from, snippet, attachments;
};

std::vector<Expect> loadExpected(const char* file = "expected.tsv") {
  std::ifstream in(fs::path(TEST_DATA_DIR) / file);
  std::vector<Expect> out;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::vector<std::string> cols;
    std::stringstream ss(line);
    for (std::string c; std::getline(ss, c, '\t');) cols.push_back(c);
    cols.resize(5);  // пустые хвостовые колонки
    out.push_back({cols[0], cols[1], cols[2], cols[3], cols[4]});
  }
  return out;
}

}  // namespace

TEST(corpus, has_at_least_30_messages_with_non_utf8_and_nested_multipart) {
  const auto expected = loadExpected();
  CHECK(expected.size() >= 30);
  MaildirStore store(corpusDir());
  CHECK(store.list().size() >= 30);

  int nonUtf8 = 0, nested = 0, embedded = 0;
  for (const auto& e : store.list()) {
    const std::string raw = store.readRaw(e.id);
    for (const char* cs : {"koi8-r", "windows-1251", "iso-8859-1", "iso-8859-2", "shift_jis", "gb2312", "big5", "iso-2022-jp"})
      if (raw.find(std::string("charset=") + cs) != std::string::npos) {
        ++nonUtf8;
        break;
      }
    if (raw.find("multipart/related") != std::string::npos || raw.find("MIX21B") != std::string::npos) ++nested;
    if (raw.find("message/rfc822") != std::string::npos) ++embedded;
  }
  CHECK(nonUtf8 >= 8);
  CHECK(nested >= 2);
  CHECK(embedded >= 2);
}

TEST(corpus, every_message_parses_and_matches_expectations) {
  Env env;
  MaildirStore store(corpusDir());
  MessageRenderer renderer(env.handlers, env.conv);
  std::set<std::string> checked;

  for (const Expect& e : loadExpected()) {
    const Message msg = MimeParser::parse(store.readRaw(e.id));
    const MessageSummary s = makeSummary(e.id, {}, msg.headers(), env.conv);
    CHECK_EQ(s.subject, e.subject);
    CHECK_EQ(shortSender(s.from), e.from);

    const RenderedMessage r = renderer.render(msg);
    if (r.body.find(e.snippet) == std::string::npos)
      testing::fail(__FILE__, __LINE__, e.id + ": в теле нет «" + e.snippet + "»; тело: «" + r.body + "»");

    std::string atts;
    for (const PartEntry& p : r.parts)
      if (p.leaf && p.attachment) atts += (atts.empty() ? "" : "|") + p.filename;
    CHECK_EQ(atts, e.attachments);
    checked.insert(e.id);
  }
  CHECK_EQ(checked.size(), store.list().size());  // ни одно письмо ящика не осталось без проверки
}

TEST(corpus, part_tree_of_deeply_nested_message) {
  Env env;
  MaildirStore store(corpusDir());
  MessageRenderer renderer(env.handlers, env.conv);
  const RenderedMessage r = renderer.render(MimeParser::parse(store.readRaw("1790000020.M20P1.testhost")));
  std::vector<std::string> labels;
  int maxDepth = 0;
  for (const PartEntry& p : r.parts) {
    maxDepth = std::max(maxDepth, p.depth);
    labels.push_back(p.label);
  }
  CHECK_EQ(r.parts.front().label, std::string("multipart/mixed"));
  CHECK_EQ(maxDepth, 3);  // mixed > alternative > related > image
  CHECK_EQ(r.parts.size(), size_t{7});
}

TEST(corpus, embedded_message_is_listed_with_its_subject) {
  Env env;
  MaildirStore store(corpusDir());
  MessageRenderer renderer(env.handlers, env.conv);
  const RenderedMessage r = renderer.render(MimeParser::parse(store.readRaw("1790000022.M22P1.testhost")));
  bool found = false;
  for (const PartEntry& p : r.parts)
    if (p.label == "message/rfc822: Исходное письмо") found = true;
  CHECK(found);
  CHECK(r.body.find("fwd html") == std::string::npos);  // тело берётся из внешнего письма, не из вложенного
}

TEST(corpus, rendered_headers_are_decoded) {
  Env env;
  MaildirStore store(corpusDir());
  MessageRenderer renderer(env.handlers, env.conv);
  const RenderedMessage r = renderer.render(MimeParser::parse(store.readRaw("1790000004.M4P1.testhost")));
  CHECK_EQ(r.headers.at(0).first, std::string("From"));
  CHECK_EQ(r.headers.at(0).second, std::string("Иван Петров <ivan@example.ru>"));
  CHECK_EQ(r.headers.at(3).second, std::string("Привет из KOI8-R"));
}
