// Корень композиции: единственное место, где собираются все слои вместе.
#include <langinfo.h>

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "core/application/commands.h"
#include "core/application/content_handlers.h"
#include "core/application/key_bindings.h"
#include "core/application/session.h"
#include "core/application/store_registry.h"
#include "core/domain/errors.h"
#include "core/domain/filters.h"
#include "core/domain/sorting.h"
#include "core/domain/summary.h"
#include "core/domain/text_util.h"
#include "infrastructure/builtin_stores.h"
#include "infrastructure/iconv_converter.h"
#include "infrastructure/sqlite_index.h"
#include "presentation/app.h"

namespace {

namespace fs = std::filesystem;

struct Options {
  std::string type = "maildir";
  std::string location;
  std::string dbPath;
  std::string keysPath;
  int refreshSeconds = 60;
  bool listOnly = false;
};

void usage() {
  std::puts(
      "Использование: maills [параметры] <путь к Maildir>\n"
      "  --type T       тип хранилища (по умолчанию maildir)\n"
      "  --db FILE      файл индекса SQLite (по умолчанию ~/.cache/maills/<ящик>.sqlite)\n"
      "  --keys FILE    конфиг клавиш (по умолчанию ~/.config/maills/keys.conf)\n"
      "  --refresh SEC  автообновление, секунд; 0 — только по клавише R (по умолчанию 60)\n"
      "  --list         один раз синхронизировать, напечатать список писем и выйти\n"
      "  -h, --help     эта справка");
}

std::string homeDir() {
  const char* h = std::getenv("HOME");
  return h ? h : ".";
}

std::string defaultDbPath(const std::string& location) {
  const char* xdg = std::getenv("XDG_CACHE_HOME");
  const fs::path dir = (xdg && *xdg ? fs::path(xdg) : fs::path(homeDir()) / ".cache") / "maills";
  fs::create_directories(dir);
  std::string name;
  std::error_code ec;
  const fs::path abs = fs::absolute(location, ec);
  for (char c : abs.string()) name += (std::isalnum(static_cast<unsigned char>(c)) ? c : '_');
  return (dir / (name + ".sqlite")).string();
}

std::string defaultKeysPath() {
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  return ((xdg && *xdg ? fs::path(xdg) : fs::path(homeDir()) / ".config") / "maills" / "keys.conf").string();
}

bool parseArgs(int argc, char** argv, Options& o) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto value = [&](std::string& dst) {
      if (i + 1 >= argc) throw mail::MailError("у параметра " + a + " нет значения");
      dst = argv[++i];
    };
    if (a == "-h" || a == "--help") return false;
    else if (a == "--type") value(o.type);
    else if (a == "--db") value(o.dbPath);
    else if (a == "--keys") value(o.keysPath);
    else if (a == "--refresh") {
      std::string v;
      value(v);
      o.refreshSeconds = std::atoi(v.c_str());
    } else if (a == "--list") o.listOnly = true;
    else if (!a.empty() && a[0] == '-') throw mail::MailError("неизвестный параметр: " + a);
    else o.location = a;
  }
  if (o.location.empty()) throw mail::MailError("не указан путь к ящику (см. --help)");
  return true;
}

void printList(const mail::MailboxSession& s) {
  for (const mail::ListRow& r : s.rows()) {
    const std::string indent(static_cast<size_t>(r.depth) * 2, ' ');
    if (r.index < 0) {
      std::printf("   %s(письмо отсутствует в ящике)\n", indent.c_str());
      continue;
    }
    const mail::MessageSummary& m = s.messages()[static_cast<size_t>(r.index)];
    char date[32] = "-";
    if (m.date != 0) {
      std::tm tmv{};
      localtime_r(&m.date, &tmv);
      std::strftime(date, sizeof date, "%Y-%m-%d %H:%M", &tmv);
    }
    std::printf("%c%c %s  %s  %s%s\n", m.flags.seen ? ' ' : 'N', m.flags.flagged ? '!' : ' ', date,
                mail::fitToWidth(mail::shortSender(m.from), 22, true).c_str(), indent.c_str(), m.subject.c_str());
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::setlocale(LC_ALL, "");
  if (std::string(nl_langinfo(CODESET)) != "UTF-8") std::setlocale(LC_ALL, "C.UTF-8");  // ncurses нужен UTF-8

  try {
    Options opt;
    if (!parseArgs(argc, argv, opt)) {
      usage();
      return 0;
    }
    if (opt.dbPath.empty()) opt.dbPath = defaultDbPath(opt.location);
    if (opt.keysPath.empty()) opt.keysPath = defaultKeysPath();

    // --- сборка зависимостей ---
    mail::StoreRegistry stores;
    mail::registerBuiltinStores(stores);
    auto store = stores.create(opt.type, opt.location);
    auto index = mail::openIndex(opt.dbPath);
    mail::IconvConverter converter;

    mail::ContentHandlerRegistry handlers;
    mail::registerBuiltinHandlers(handlers);
    mail::SortRegistry sorts;
    mail::registerBuiltinSorts(sorts);
    mail::FilterRegistry filters;
    mail::registerBuiltinFilters(filters);
    mail::CommandRegistry commands;
    mail::registerBuiltinCommands(commands);

    mail::KeyBindings keys = mail::KeyBindings::withDefaults();
    for (const std::string& w : keys.loadFile(opt.keysPath, commands)) std::fprintf(stderr, "предупреждение: %s\n", w.c_str());

    mail::MailboxSession session(*store, *index, converter, {handlers, sorts, filters});

    if (opt.listOnly) {
      const mail::SyncResult r = session.syncNow();
      printList(session);
      std::fprintf(stderr, "писем: %d (новых: %d, удалено: %d), непрочитанных: %d\n", r.total, r.added, r.removed, r.unread);
      return 0;
    }

    mail::App app(session, commands, keys, opt.refreshSeconds);
    app.run();
  } catch (const mail::MailError& e) {
    std::fprintf(stderr, "Ошибка: %s\n", e.what());
    return 1;
  }
  return 0;
}
