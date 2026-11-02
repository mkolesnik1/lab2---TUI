#pragma once
#include <stdlib.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "check.h"
#include "infrastructure/maildir_store.h"
#include "infrastructure/sqlite_index.h"

namespace testutil {
namespace fs = std::filesystem;

inline fs::path corpusDir() { return fs::path(TEST_DATA_DIR) / "maildir"; }

// Временный каталог, удаляется в деструкторе.
struct TempDir {
  fs::path path;
  TempDir() {
    std::string tpl = (fs::temp_directory_path() / "maills-test-XXXXXX").string();
    path = mkdtemp(tpl.data());
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
};

inline fs::path copyCorpus(const fs::path& dst) {
  fs::copy(corpusDir(), dst, fs::copy_options::recursive);
  return dst;
}

inline std::string readFile(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

inline void writeFile(const fs::path& p, const std::string& data) {
  std::ofstream out(p, std::ios::binary);
  out << data;
}

// Находит файл письма в Maildir по id (без учёта флагов в имени).
inline fs::path findFile(const fs::path& root, const std::string& id) {
  for (const char* sub : {"cur", "new"})
    for (const auto& e : fs::directory_iterator(root / sub))
      if (e.path().filename().string().rfind(id, 0) == 0) return e.path();
  return {};
}

}  // namespace testutil
