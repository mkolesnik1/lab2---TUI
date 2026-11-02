#pragma once
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/application/session.h"

namespace mail {

// Если команде нужен текстовый аргумент (путь, строка поиска), UI показывает строку ввода.
struct Prompt {
  std::string label;
  std::string initial;
};

// Command: действие пользователя как объект. Клавиши к командам привязывает KeyBindings,
// поэтому команда ничего не знает о клавишах и терминале.
class ICommand {
 public:
  virtual ~ICommand() = default;
  virtual std::string name() const = 0;         // ключ для конфига: "toggle-flagged"
  virtual std::string description() const = 0;  // строка для экрана справки
  virtual std::optional<Prompt> prompt(const MailboxSession&) const { return std::nullopt; }
  virtual void execute(MailboxSession& session, const std::string& argument) const = 0;
};

class CommandRegistry {
 public:
  void add(std::unique_ptr<ICommand> c);
  const ICommand* find(const std::string& name) const;  // nullptr если нет
  std::vector<const ICommand*> all() const;             // в порядке добавления

 private:
  std::vector<std::unique_ptr<ICommand>> order_;
  std::map<std::string, const ICommand*> byName_;
};

// Выполняет команду; MailError превращается в сообщение строки состояния, программа продолжает работу.
void runCommand(const ICommand& command, MailboxSession& session, const std::string& argument);

void registerBuiltinCommands(CommandRegistry& r);

}  // namespace mail
