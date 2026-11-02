#include "check.h"
#include "core/application/sync_service.h"
#include "core/domain/errors.h"
#include "infrastructure/iconv_converter.h"
#include "support.h"

using namespace mail;
using namespace testutil;

namespace {

int expectedUnread(const fs::path& root) {
  int n = 0;
  for (const auto& e : fs::directory_iterator(root / "new")) n += e.is_regular_file() ? 1 : 0;
  for (const auto& e : fs::directory_iterator(root / "cur")) {
    const std::string name = e.path().filename().string();
    const size_t p = name.find(":2,");
    if (p == std::string::npos || name.find('S', p) == std::string::npos) ++n;
  }
  return n;
}

}  // namespace

TEST(sync, first_run_indexes_everything) {
  TempDir tmp;
  copyCorpus(tmp.path);
  IconvConverter conv;
  MaildirStore store(tmp.path);
  SqliteIndex index(":memory:");
  SyncService sync(store, index, conv);

  const SyncResult r = sync.run();
  CHECK_EQ(r.added, 48);
  CHECK_EQ(r.removed, 0);
  CHECK_EQ(r.total, 48);
  CHECK_EQ(r.unread, expectedUnread(tmp.path));
  CHECK_EQ(index.loadAll().size(), size_t{48});
}

TEST(sync, second_run_without_changes_does_nothing) {
  TempDir tmp;
  copyCorpus(tmp.path);
  IconvConverter conv;
  MaildirStore store(tmp.path);
  SqliteIndex index(":memory:");
  SyncService sync(store, index, conv);
  sync.run();
  const SyncResult r = sync.run();
  CHECK(r.added == 0 && r.removed == 0 && r.flagsChanged == 0);
  CHECK_EQ(r.total, 48);
}

TEST(sync, new_and_deleted_files_are_picked_up) {
  TempDir tmp;
  copyCorpus(tmp.path);
  IconvConverter conv;
  MaildirStore store(tmp.path);
  SqliteIndex index(":memory:");
  SyncService sync(store, index, conv);
  sync.run();

  writeFile(tmp.path / "new" / "1800000000.M1P1.testhost", "From: New <new@example.com>\nSubject: Brand new\n\nhi\n");
  fs::remove(findFile(tmp.path, "1790000001.M1P1.testhost"));
  const SyncResult r = sync.run();
  CHECK_EQ(r.added, 1);
  CHECK_EQ(r.removed, 1);
  CHECK_EQ(r.total, 48);

  bool found = false;
  for (const auto& m : index.loadAll()) {
    if (m.subject == "Brand new") found = true;
    CHECK(m.id != "1790000001.M1P1.testhost");
  }
  CHECK(found);
}

TEST(sync, manual_flag_edit_in_filename_is_picked_up) {
  // Пользователь переименовал файл руками (как это делает другой почтовый клиент).
  TempDir tmp;
  copyCorpus(tmp.path);
  IconvConverter conv;
  MaildirStore store(tmp.path);
  SqliteIndex index(":memory:");
  SyncService sync(store, index, conv);
  sync.run();

  const std::string id = "1790000001.M1P1.testhost";
  fs::rename(findFile(tmp.path, id), tmp.path / "cur" / (id + ":2,FS"));
  const SyncResult r = sync.run();
  CHECK_EQ(r.flagsChanged, 1);
  CHECK(index.knownFlags().at(id).flagged);
}

TEST(sync, broken_message_is_listed_not_fatal) {
  TempDir tmp;
  copyCorpus(tmp.path);
  IconvConverter conv;
  MaildirStore store(tmp.path);
  SqliteIndex index(":memory:");
  SyncService sync(store, index, conv);
  writeFile(tmp.path / "new" / "1800000001.M2P1.testhost", "");  // пустой файл
  const SyncResult r = sync.run();
  CHECK_EQ(r.failed, 1);
  CHECK_EQ(r.added, 49);
  bool found = false;
  for (const auto& m : index.loadAll())
    if (m.id == "1800000001.M2P1.testhost") found = m.subject.find("не удалось разобрать") != std::string::npos;
  CHECK(found);
}

TEST(sync, flag_update_is_compare_and_set) {
  SqliteIndex index(":memory:");
  MessageSummary m;
  m.id = "a";
  m.flags.seen = true;
  IndexChanges add;
  add.added.push_back(m);
  index.apply(add, SyncLogEntry{1, 1, 0, 1});

  Flags flagged;
  flagged.seen = true;
  flagged.flagged = true;
  IndexChanges stale;  // «ожидал unseen», а в индексе уже seen -> изменение не применяется
  stale.flags.push_back({"a", Flags{}, flagged});
  index.apply(stale, SyncLogEntry{2, 0, 0, 1});
  CHECK(!index.knownFlags().at("a").flagged);

  IndexChanges fresh;
  Flags seen;
  seen.seen = true;
  fresh.flags.push_back({"a", seen, flagged});
  index.apply(fresh, SyncLogEntry{3, 0, 0, 1});
  CHECK(index.knownFlags().at("a").flagged);
}

TEST(sync, history_is_newest_first) {
  SqliteIndex index(":memory:");
  index.apply({}, SyncLogEntry{100, 5, 0, 5});
  index.apply({}, SyncLogEntry{200, 2, 1, 6});
  const auto h = index.syncHistory(5);
  CHECK_EQ(h.size(), size_t{2});
  CHECK_EQ(h[0].total, 6);
  CHECK_EQ(h[1].total, 5);
  CHECK_EQ(index.syncHistory(1).size(), size_t{1});
}

TEST(sync, index_persists_between_runs_and_recovers_from_corruption) {
  TempDir tmp;
  const std::string db = (tmp.path / "index.sqlite").string();
  {
    auto idx = openIndex(db);
    MessageSummary m;
    m.id = "kept";
    IndexChanges c;
    c.added.push_back(m);
    idx->apply(c, SyncLogEntry{1, 1, 0, 1});
  }
  CHECK_EQ(openIndex(db)->loadAll().size(), size_t{1});  // данные пережили «перезапуск»

  writeFile(db, "this is definitely not an sqlite file, someone edited it by hand");
  auto recovered = openIndex(db);  // не падаем: битый кэш откладывается в *.broken и создаётся заново
  CHECK_EQ(recovered->loadAll().size(), size_t{0});
  CHECK(fs::exists(db + ".broken"));
}
