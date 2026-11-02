#include "check.h"
#include "core/application/content_handlers.h"

using namespace mail;

TEST(handlers, html_strips_tags_script_style_and_head) {
  CHECK_EQ(htmlToText("<html><head><title>T</title></head><body><style>p{}</style>Hello<script>x()</script></body></html>"),
           std::string("Hello"));
}

TEST(handlers, html_block_tags_become_line_breaks) {
  CHECK_EQ(htmlToText("<p>one</p><p>two</p>"), std::string("one\n\ntwo"));
  CHECK_EQ(htmlToText("a<br>b<br/>c"), std::string("a\nb\nc"));
  CHECK_EQ(htmlToText("<div>x</div><div>y</div>"), std::string("x\ny"));
  CHECK_EQ(htmlToText("<ul><li>a</li><li>b</li></ul>"), std::string("• a\n• b"));
}

TEST(handlers, html_entities) {
  CHECK_EQ(htmlToText("a &amp; b &lt;c&gt; &quot;d&quot;"), std::string("a & b <c> \"d\""));
  CHECK_EQ(htmlToText("&#1055;&#x44F;"), std::string("Пя"));
  CHECK_EQ(htmlToText("x&nbsp;y"), std::string("x y"));
  CHECK_EQ(htmlToText("AT&T &unknown; & co"), std::string("AT&T &unknown; & co"));  // неизвестное оставляем
}

TEST(handlers, html_whitespace_comments_and_unknown_tags) {
  CHECK_EQ(htmlToText("a   \n  b <!-- hidden --> <custom-tag attr=1>c</custom-tag>"), std::string("a b c"));
  CHECK_EQ(htmlToText("1 < 2"), std::string("1 < 2"));
}

TEST(handlers, registry_finds_handler_by_mime_type) {
  ContentHandlerRegistry r;
  registerBuiltinHandlers(r);
  CHECK(r.find("text/plain") != nullptr);
  CHECK(r.find("text/html") != nullptr);
  CHECK(r.find("text/calendar") != nullptr);
  CHECK(r.find("image/png") == nullptr);
  CHECK(r.find("text/plain")->priority() < r.find("text/html")->priority());
}

TEST(handlers, calendar_invite_becomes_readable_text) {
  const std::string ics =
      "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nSUMMARY:Встреча\\, обсудить \r\n план\r\n"  // перенос строки: пробел продолжения убирается
      "DTSTART;TZID=UTC:20261105T100000Z\r\nDTEND:20261105\r\nLOCATION:Каб. 5\r\n"
      "ORGANIZER;CN=Аня:mailto:anya@example.org\r\nDESCRIPTION:строка1\\nстрока2\r\n"
      "END:VEVENT\r\nEND:VCALENDAR\r\n";
  CHECK_EQ(icalToText(ics),
           std::string("Событие: Встреча, обсудить план\nНачало: 2026-11-05 10:00 UTC\nКонец: 2026-11-05\n"
                       "Место: Каб. 5\nОрганизатор: anya@example.org\nОписание: строка1\nстрока2"));
  CHECK_EQ(icalToText("просто текст"), std::string("просто текст"));  // не календарь — как есть
}
