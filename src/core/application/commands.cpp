#include "core/application/commands.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>

#include "core/domain/errors.h"

namespace mail {

void CommandRegistry::add(std::unique_ptr<ICommand> c) {
  byName_[c->name()] = c.get();
  order_.push_back(std::move(c));
}

const ICommand* CommandRegistry::find(const std::string& name) const {
  auto it = byName_.find(name);
  return it == byName_.end() ? nullptr : it->second;
}

std::vector<const ICommand*> CommandRegistry::all() const {
  std::vector<const ICommand*> out;
  for (const auto& c : order_) out.push_back(c.get());
  return out;
}

void runCommand(const ICommand& command, MailboxSession& session, const std::string& argument) {
  try {
    command.execute(session, argument);
  } catch (const MailError& e) {
    session.setStatus(e.what(), true);
  } catch (const std::exception& e) {
    session.setStatus(std::string("внутренняя ошибка: ") + e.what(), true);
  }
}

namespace {

namespace fs = std::filesystem;

// Простая команда из функции.
class SimpleCommand final : public ICommand {
 public:
  SimpleCommand(std::string name, std::string description, std::function<void(MailboxSession&)> fn)
      : name_(std::move(name)), description_(std::move(description)), fn_(std::move(fn)) {}
  std::string name() const override { return name_; }
  std::string description() const override { return description_; }
  void execute(MailboxSession& s, const std::string&) const override { fn_(s); }

 private:
  std::string name_, description_;
  std::function<void(MailboxSession&)> fn_;
};

// Запись нового файла: "~/" раскрывается, каталог -> имя добавляется, существующие файлы не затираем.
void writeNewFile(const std::string& userPath, const std::string& defaultName, const std::string& bytes) {
  std::string p = userPath;
  if (p.empty()) p = defaultName;
  if (p.size() >= 2 && p[0] == '~' && p[1] == '/')
    if (const char* home = std::getenv("HOME")) p = std::string(home) + p.substr(1);
  fs::path path(p);
  std::error_code ec;
  if (fs::is_directory(path, ec)) path /= defaultName;
  if (path.empty() || path.filename().empty()) throw StoreError("не указано имя файла");
  if (fs::exists(path, ec)) throw StoreError("файл уже существует: " + path.string());
  std::ofstream out(path, std::ios::binary);
  if (!out) throw StoreError("не удалось создать файл: " + path.string());
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  out.flush();
  if (!out) throw StoreError("ошибка записи: " + path.string());
}

class SearchCommand final : public ICommand {
 public:
  std::string name() const override { return "search"; }
  std::string description() const override { return "Поиск по отправителю и теме (from:, subject:, is:unread)"; }
  std::optional<Prompt> prompt(const MailboxSession& s) const override {
    return Prompt{"Поиск: ", s.filterText()};
  }
  void execute(MailboxSession& s, const std::string& arg) const override { s.setFilter(arg); }
};

class SaveAttachmentCommand final : public ICommand {
 public:
  std::string name() const override { return "save-attachment"; }
  std::string description() const override { return "Сохранить вложение (часть под курсором) в файл"; }
  std::optional<Prompt> prompt(const MailboxSession& s) const override {
    return Prompt{"Сохранить как: ", s.suggestedAttachmentName()};
  }
  void execute(MailboxSession& s, const std::string& arg) const override {
    const PartEntry* part = s.targetAttachment();
    if (!part) throw StoreError("в письме нет вложений");
    if (!part->leaf) throw StoreError("выберите часть-лист (не контейнер) в списке частей (p)");
    writeNewFile(arg, s.suggestedAttachmentName(), part->leaf->decodedBody());
    s.setStatus("Вложение сохранено");
  }
};

class SaveTextCommand final : public ICommand {
 public:
  std::string name() const override { return "save-text"; }
  std::string description() const override { return "Сохранить текст письма (UTF-8) в файл"; }
  std::optional<Prompt> prompt(const MailboxSession& s) const override {
    return Prompt{"Сохранить текст как: ", s.suggestedBaseName() + ".txt"};
  }
  void execute(MailboxSession& s, const std::string& arg) const override {
    writeNewFile(arg, s.suggestedBaseName() + ".txt", s.currentBodyText());
    s.setStatus("Текст сохранён");
  }
};

class ExportEmlCommand final : public ICommand {
 public:
  std::string name() const override { return "export-eml"; }
  std::string description() const override { return "Экспортировать письмо целиком в .eml"; }
  std::optional<Prompt> prompt(const MailboxSession& s) const override {
    return Prompt{"Экспорт в .eml: ", s.suggestedBaseName() + ".eml"};
  }
  void execute(MailboxSession& s, const std::string& arg) const override {
    writeNewFile(arg, s.suggestedBaseName() + ".eml", s.currentRaw());
    s.setStatus("Письмо экспортировано");
  }
};

}  // namespace

void registerBuiltinCommands(CommandRegistry& r) {
  auto simple = [&r](std::string name, std::string desc, std::function<void(MailboxSession&)> fn) {
    r.add(std::make_unique<SimpleCommand>(std::move(name), std::move(desc), std::move(fn)));
  };
  simple("quit", "Выход", [](MailboxSession& s) { s.requestQuit(); });
  simple("back-or-quit", "Назад; из списка — выход", [](MailboxSession& s) {
    if (s.mode() == Mode::List) s.requestQuit();
    else s.back();
  });
  simple("back", "Назад", [](MailboxSession& s) { s.back(); });
  simple("down", "Вниз", [](MailboxSession& s) { s.move(1); });
  simple("up", "Вверх", [](MailboxSession& s) { s.move(-1); });
  simple("page-down", "Страница вниз", [](MailboxSession& s) { s.page(1); });
  simple("page-up", "Страница вверх", [](MailboxSession& s) { s.page(-1); });
  simple("top", "В начало", [](MailboxSession& s) { s.moveToEdge(true); });
  simple("bottom", "В конец", [](MailboxSession& s) { s.moveToEdge(false); });
  simple("open", "Открыть письмо", [](MailboxSession& s) { s.openSelected(); });
  simple("help", "Справка", [](MailboxSession& s) { s.showHelp(); });
  simple("refresh", "Обновить: подгрузить новые письма", [](MailboxSession& s) { s.refresh(); });
  simple("toggle-flagged", "Пометить / снять пометку (флаг F)", [](MailboxSession& s) { s.toggleFlagged(); });
  simple("toggle-seen", "Прочитано / непрочитано (флаг S)", [](MailboxSession& s) { s.toggleSeen(); });
  simple("sort-date", "Сортировка по дате", [](MailboxSession& s) { s.setSort("date"); });
  simple("sort-sender", "Сортировка по отправителю", [](MailboxSession& s) { s.setSort("sender"); });
  simple("sort-subject", "Сортировка по теме", [](MailboxSession& s) { s.setSort("subject"); });
  simple("reverse-sort", "Обратный порядок", [](MailboxSession& s) { s.toggleReverse(); });
  simple("toggle-threads", "Треды деревом / плоский список", [](MailboxSession& s) { s.toggleThreads(); });
  simple("clear-filter", "Сбросить поиск", [](MailboxSession& s) { s.setFilter(""); });
  simple("toggle-parts", "Дерево частей письма / текст", [](MailboxSession& s) { s.togglePartsView(); });
  r.add(std::make_unique<SearchCommand>());
  r.add(std::make_unique<SaveAttachmentCommand>());
  r.add(std::make_unique<SaveTextCommand>());
  r.add(std::make_unique<ExportEmlCommand>());
}

}  // namespace mail
