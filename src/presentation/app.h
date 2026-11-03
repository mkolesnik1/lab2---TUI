#pragma once
#include <chrono>
#include <map>
#include <memory>
#include <optional>

#include "core/application/commands.h"
#include "core/application/key_bindings.h"
#include "core/application/session.h"
#include "presentation/terminal.h"
#include "presentation/views.h"

namespace mail {

// Главный цикл TUI: клавиши -> KeyBindings -> CommandRegistry -> команда -> сеанс; затем перерисовка.
// Логики предметной области здесь нет.
class App {
 public:
  App(MailboxSession& session, const CommandRegistry& commands, const KeyBindings& keys, int autoRefreshSeconds);
  void run();

 private:
  struct PromptState {
    const ICommand* command;
    PromptView view;
  };
  void drawFrame();
  void handleKey(const Key& key);
  void handlePromptKey(const Key& key);

  MailboxSession& session_;
  const CommandRegistry& commands_;
  const KeyBindings& keys_;
  std::map<Mode, std::unique_ptr<IView>> views_;
  std::optional<PromptState> prompt_;
  int autoRefreshSeconds_;
  std::chrono::steady_clock::time_point lastRefresh_ = std::chrono::steady_clock::now();
};

}  // namespace mail
