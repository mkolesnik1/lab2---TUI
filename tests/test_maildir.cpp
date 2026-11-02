#include <algorithm>
#include <map>

#include "check.h"
#include "core/domain/errors.h"
#include "support.h"

using namespace mail;
using namespace testutil;

namespace {
std::map<StoreId, Flags> listAll(const MaildirStore& s) {
  std::map<StoreId, Flags> m;
  for (const auto& e : s.list()) m[e.id] = e.flags;
  return m;
}
}  // namespace

TEST(maildir, flags_letters_are_sorted_alphabetically) {
  Flags f;
  f.seen = f.flagged = f.replied = true;
  CHECK_EQ(f.toMaildir(), std::string("FRS"));
  f.trashed = f.draft = true;
  CHECK_EQ(f.toMaildir(), std::string("DFRST"));
  const Flags g = Flags::fromMaildir("SRFP");  // порядок на входе не важен, P игнорируется
  CHECK(g.seen && g.replied && g.flagged && !g.trashed && !g.draft);
}

TEST(maildir, lists_new_and_cur_and_parses_flags_from_filename) {
  TempDir tmp;
  copyCorpus(tmp.path);
  MaildirStore store(tmp.path);
  const auto all = listAll(store);
  CHECK_EQ(all.size(), size_t{48});
  const auto& m3 = all.at("1790000003.M3P1.testhost");  // :2,FS
  CHECK(m3.flagged && m3.seen && !m3.replied);
  const auto& m45 = all.at("1790000045.M45P1.testhost");  // :2,FRS
  CHECK(m45.flagged && m45.replied && m45.seen);
  CHECK(all.at("1790000047.M47P1.testhost").trashed);
  CHECK(!all.at("1790000012.M12P1.testhost").seen);  // лежит в new/
}

TEST(maildir, set_flags_renames_file_and_moves_new_to_cur) {
  TempDir tmp;
  copyCorpus(tmp.path);
  MaildirStore store(tmp.path);
  const std::string id = "1790000012.M12P1.testhost";
  CHECK(fs::exists(tmp.path / "new" / id));

  Flags f;
  f.seen = true;
  f.flagged = true;
  store.setFlags(id, f);
  CHECK(!fs::exists(tmp.path / "new" / id));
  CHECK(fs::exists(tmp.path / "cur" / (id + ":2,FS")));
  CHECK(listAll(store).at(id) == f);  // id не изменился, флаги читаются обратно

  f.flagged = false;  // повторная смена: только суффикс
  store.setFlags(id, f);
  CHECK(fs::exists(tmp.path / "cur" / (id + ":2,S")));
  CHECK(!fs::exists(tmp.path / "cur" / (id + ":2,FS")));
  CHECK_EQ(store.readRaw(id).substr(0, 5), std::string("From:"));  // читается и после переименований
}

TEST(maildir, set_same_flags_is_noop) {
  TempDir tmp;
  copyCorpus(tmp.path);
  MaildirStore store(tmp.path);
  const std::string id = "1790000001.M1P1.testhost";  // :2,S
  Flags f;
  f.seen = true;
  store.setFlags(id, f);
  CHECK(fs::exists(tmp.path / "cur" / (id + ":2,S")));
}

TEST(maildir, errors_are_store_errors) {
  TempDir tmp;
  CHECK_THROWS(MaildirStore(tmp.path / "no-such-dir"), StoreError);
  CHECK_THROWS(MaildirStore(tmp.path), StoreError);  // каталог есть, но это не Maildir
  copyCorpus(tmp.path / "box");
  MaildirStore store(tmp.path / "box");
  CHECK_THROWS(store.readRaw("nope"), StoreError);
  CHECK_THROWS(store.setFlags("nope", Flags{}), StoreError);
}

TEST(maildir, hidden_files_and_subdirectories_are_ignored) {
  TempDir tmp;
  copyCorpus(tmp.path);
  writeFile(tmp.path / "cur" / ".hidden", "x");
  fs::create_directory(tmp.path / "cur" / "subdir");
  MaildirStore store(tmp.path);
  CHECK_EQ(store.list().size(), size_t{48});
}
