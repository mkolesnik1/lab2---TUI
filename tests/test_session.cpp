#include <thread>

#include "check.h"
#include "core/application/key_bindings.h"
#include "core/domain/errors.h"
#include "core/domain/text_util.h"
#include "env.h"

using namespace mail;
using namespace testutil;

namespace {

struct Fixture {
  TempDir tmp, out;
  Env env;
  std::unique_ptr<MaildirStore> store;
  SqliteIndex index{":memory:"};
  std::unique_ptr<MailboxSession> session;
  Fixture() {
    copyCorpus(tmp.path);
    store = std::make_unique<MaildirStore>(tmp.path);
    session = std::make_unique<MailboxSession>(*store, index, env.conv, env.services());
    session->syncNow();
  }
  void run(const std::string& command, const std::string& arg = "") {
    const ICommand* c = env.commands.find(command);
    CHECK(c != nullptr);
    if (c) runCommand(*c, *session, arg);
  }
};

}  // namespace

TEST(session, threaded_list_contains_all_messages_plus_placeholder) {
  Fixture f;
  int placeholders = 0, messages = 0;
  for (const ListRow& r : f.session->rows()) (r.index < 0 ? placeholders : messages)++;
  CHECK_EQ(messages, 48);
  CHECK_EQ(placeholders, 1);  // <missing@t> — родитель письма «Re: Lost thread»
}

TEST(session, search_by_sender_and_subject_is_case_insensitive_and_unicode_aware) {
  Fixture f;
  f.session->setFilter("ИВАН");  // «Иван Петров» и «Иван Иванов»
  CHECK_EQ(f.session->rows().size(), size_t{2});
  f.session->setFilter("from:alice");
  CHECK_EQ(f.session->rows().size(), size_t{2});  // Alice Smith и Alice (тред)
  f.session->setFilter("subject:PROJECT");
  CHECK_EQ(f.session->rows().size(), size_t{4});
  f.session->setFilter("отчёт");  // без префикса: отправитель ИЛИ тема, тема раскодирована из RFC 2047
  CHECK(f.session->rows().size() >= 1);
  f.session->setFilter("is:unread");
  CHECK_EQ(f.session->rows().size(), static_cast<size_t>(f.session->unreadCount()));
  f.session->setFilter("");
  CHECK_EQ(f.session->rows().size(), size_t{49});
}

TEST(session, sorting_changes_order_and_keeps_selection) {
  Fixture f;
  CHECK(selectById(*f.session, "1790000004.M4P1.testhost"));
  f.session->setSort("sender");
  CHECK_EQ(f.session->selected()->id, std::string("1790000004.M4P1.testhost"));  // выбранное письмо не потерялось
  f.session->toggleThreads();  // плоский список
  std::string prev;
  for (const ListRow& r : f.session->rows()) {
    const std::string cur = lowerUtf8(shortSender(f.session->messages()[static_cast<size_t>(r.index)].from));
    CHECK(prev <= cur);
    prev = cur;
  }
  f.session->setSort("nonsense");
  CHECK(f.session->status().error);
  CHECK_EQ(f.session->sortName(), std::string("sender"));
}

TEST(session, opening_unread_message_marks_it_seen_in_filename) {
  Fixture f;
  const std::string id = "1790000012.M12P1.testhost";  // лежит в new/
  CHECK(selectById(*f.session, id));
  f.run("open");
  CHECK(f.session->mode() == Mode::Message);
  CHECK(f.session->message() != nullptr);
  CHECK(f.session->message()->body.find("Привет, мир & друзья") != std::string::npos);
  CHECK(!fs::exists(f.tmp.path / "new" / id));
  CHECK(fs::exists(f.tmp.path / "cur" / (id + ":2,S")));
  CHECK(f.session->selected()->flags.seen);
  f.run("back");
  CHECK(f.session->mode() == Mode::List);
  CHECK(f.session->message() == nullptr);
}

TEST(session, toggle_flagged_and_seen_are_stored_in_filename_and_index) {
  Fixture f;
  const std::string id = "1790000001.M1P1.testhost";  // :2,S
  CHECK(selectById(*f.session, id));
  f.run("toggle-flagged");
  CHECK(fs::exists(f.tmp.path / "cur" / (id + ":2,FS")));
  CHECK(f.index.knownFlags().at(id).flagged);
  f.run("toggle-seen");
  CHECK(fs::exists(f.tmp.path / "cur" / (id + ":2,F")));
  f.run("toggle-flagged");
  f.run("toggle-seen");
  CHECK(fs::exists(f.tmp.path / "cur" / (id + ":2,S")));
}

TEST(session, errors_are_reported_in_status_bar_without_crashing) {
  Fixture f;
  const std::string id = "1790000001.M1P1.testhost";
  CHECK(selectById(*f.session, id));
  fs::remove(findFile(f.tmp.path, id));  // письмо исчезло, пока открыт список
  f.run("open");
  CHECK(f.session->status().error);
  CHECK(f.session->mode() == Mode::List);
  f.run("toggle-flagged");
  CHECK(f.session->status().error);
  f.run("export-eml", (f.out.path / "x.eml").string());
  CHECK(f.session->status().error);
}

TEST(session, save_attachment_writes_decoded_bytes_and_refuses_overwrite) {
  Fixture f;
  CHECK(selectById(*f.session, "1790000017.M17P1.testhost"));  // два вложения
  f.run("open");
  f.run("toggle-parts");
  CHECK(f.session->showParts());
  const RenderedMessage* m = f.session->message();
  CHECK(m != nullptr);
  // курсор на первое вложение (текстовое «отчёт.txt»)
  size_t idx = 0;
  while (idx < m->parts.size() && !m->parts[idx].attachment) ++idx;
  while (f.session->partCursor() < idx) f.session->move(1);
  CHECK_EQ(f.session->suggestedAttachmentName(), std::string("отчёт.txt"));

  const std::string target = (f.out.path / "saved.txt").string();
  f.run("save-attachment", target);
  CHECK(!f.session->status().error);
  CHECK_EQ(readFile(target), std::string("Содержимое отчёта\n"));

  f.run("save-attachment", target);  // файл уже есть
  CHECK(f.session->status().error);

  f.run("save-attachment", f.out.path.string());  // каталог: имя берётся из письма
  CHECK(fs::exists(f.out.path / "отчёт.txt"));
}

TEST(session, save_text_and_export_eml) {
  Fixture f;
  const std::string id = "1790000004.M4P1.testhost";
  CHECK(selectById(*f.session, id));
  f.run("open");
  f.run("save-text", (f.out.path / "body.txt").string());
  CHECK(readFile(f.out.path / "body.txt").find("кодировке KOI8-R") != std::string::npos);  // UTF-8 после перекодировки
  f.run("export-eml", (f.out.path / "mail.eml").string());
  CHECK_EQ(readFile(f.out.path / "mail.eml"), readFile(findFile(f.tmp.path, id)));  // побайтово исходное письмо
}

TEST(session, navigation_is_clamped) {
  Fixture f;
  f.session->setPageSize(10);
  f.run("up");
  CHECK_EQ(f.session->cursor(), size_t{0});
  f.run("bottom");
  CHECK_EQ(f.session->cursor(), f.session->rows().size() - 1);
  f.run("down");
  CHECK_EQ(f.session->cursor(), f.session->rows().size() - 1);
  f.run("top");
  f.run("page-down");
  CHECK_EQ(f.session->cursor(), size_t{9});
}

TEST(session, help_mode_returns_to_previous_mode) {
  Fixture f;
  CHECK(selectById(*f.session, "1790000001.M1P1.testhost"));
  f.run("open");
  f.run("help");
  CHECK(f.session->mode() == Mode::Help);
  f.run("back");
  CHECK(f.session->mode() == Mode::Message);
  f.run("back-or-quit");
  CHECK(f.session->mode() == Mode::List);
  CHECK(!f.session->quitRequested());
  f.run("back-or-quit");
  CHECK(f.session->quitRequested());
}

TEST(session, background_refresh_picks_up_new_mail_without_blocking) {
  Fixture f;
  writeFile(f.tmp.path / "new" / "1800000005.M5P1.testhost", "From: Late <late@example.com>\nSubject: Late mail\n\nx\n");
  f.run("refresh");
  for (int i = 0; i < 200 && f.session->syncRunning(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
  f.session->tick();
  CHECK(f.session->status().text.find("+1 новых") != std::string::npos);
  CHECK_EQ(f.session->messages().size(), size_t{49});
}

TEST(session, key_bindings_defaults_overrides_and_bad_config) {
  Env env;
  KeyBindings keys = KeyBindings::withDefaults();
  CHECK_EQ(*keys.commandFor("Down"), std::string("down"));
  CHECK_EQ(*keys.commandFor("R"), std::string("refresh"));

  std::istringstream cfg(
      "# comment\n"
      "Down = page-down\n"
      "x = toggle-seen\n"
      "Up = none\n"
      "z = no-such-command\n"
      "garbage line\n"
      "= = quit\n");
  const auto warnings = keys.load(cfg, env.commands);
  CHECK_EQ(warnings.size(), size_t{2});  // неизвестная команда и строка без «=»; программа продолжает работу
  CHECK_EQ(*keys.commandFor("Down"), std::string("page-down"));
  CHECK_EQ(*keys.commandFor("x"), std::string("toggle-seen"));
  CHECK(!keys.commandFor("Up"));
  CHECK_EQ(*keys.commandFor("="), std::string("quit"));
  CHECK(keys.loadFile("/no/such/file", env.commands).empty());  // нет файла — значения по умолчанию
}
