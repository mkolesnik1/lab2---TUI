#include "infrastructure/maildir_store.h"

#include <fstream>
#include <iterator>

#include "core/domain/errors.h"

namespace mail {
namespace fs = std::filesystem;

namespace {

// "1699.M1.host:2,FS" -> id "1699.M1.host"
StoreId idOf(const std::string& filename) { return filename.substr(0, filename.find(':')); }

Flags flagsOf(const std::string& filename) {
  const size_t colon = filename.find(':');
  if (colon == std::string::npos || filename.compare(colon, 3, ":2,") != 0) return {};
  return Flags::fromMaildir(std::string_view(filename).substr(colon + 3));
}

}  // namespace

MaildirStore::MaildirStore(fs::path root) : root_(std::move(root)) {
  std::error_code ec;
  if (!fs::is_directory(root_ / "cur", ec) || !fs::is_directory(root_ / "new", ec))
    throw StoreError("не Maildir (нет каталогов cur/ и new/): " + root_.string());
}

std::vector<StoredMessage> MaildirStore::list() const {
  std::vector<StoredMessage> out;
  for (const char* sub : {"new", "cur"}) {
    std::error_code ec;
    for (fs::directory_iterator it(root_ / sub, ec), end; !ec && it != end; it.increment(ec)) {
      const std::string name = it->path().filename().string();
      if (name.empty() || name[0] == '.' || !it->is_regular_file(ec)) continue;
      StoredMessage m;
      m.id = idOf(name);
      if (std::string_view(sub) == "cur") m.flags = flagsOf(name);  // в new/ флаги не действуют
      out.push_back(std::move(m));
    }
    if (ec) throw StoreError("не удалось прочитать " + (root_ / sub).string() + ": " + ec.message());
  }
  return out;
}

fs::path MaildirStore::findPath(const StoreId& id) const {
  for (const char* sub : {"cur", "new"}) {
    std::error_code ec;
    for (fs::directory_iterator it(root_ / sub, ec), end; !ec && it != end; it.increment(ec))
      if (idOf(it->path().filename().string()) == id) return it->path();
  }
  throw StoreError("письмо не найдено в хранилище: " + id);
}

std::string MaildirStore::readRaw(const StoreId& id) const {
  const fs::path path = findPath(id);
  std::ifstream in(path, std::ios::binary);
  if (!in) throw StoreError("не удалось открыть файл: " + path.string());
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void MaildirStore::setFlags(const StoreId& id, Flags flags) {
  const fs::path from = findPath(id);
  const fs::path to = root_ / "cur" / (id + ":2," + flags.toMaildir());  // письмо из new/ переезжает в cur/
  if (from == to) return;
  std::error_code ec;
  if (fs::exists(to, ec)) throw StoreError("файл уже существует: " + to.string());
  fs::rename(from, to, ec);  // атомарно в пределах одной ФС
  if (ec) throw StoreError("не удалось изменить флаги (" + from.string() + "): " + ec.message());
}

}  // namespace mail
