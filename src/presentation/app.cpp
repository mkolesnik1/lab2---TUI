#include "presentation/app.h"

#include "core/domain/text_util.h"

namespace mail {

App::App(MailboxSession& session, const CommandRegistry& commands, const KeyBindings& keys, int autoRefreshSeconds)
    : session_(session), commands_(commands), keys_(keys), autoRefreshSeconds_(autoRefreshSeconds) {
  views_[Mode::List] = std::make_unique<MailListView>();
  views_[Mode::Message] = std::make_unique<MessageView>();
  views_[Mode::Help] = std::make_unique<HelpView>(commands, keys);
}

void App::run() {
  Terminal terminal;  // ncurses живёт, пока жив этот объект
  session_.start();   // кэш из SQLite показываем сразу, синхронизация идёт в фоне
  while (!session_.quitRequested()) {
    session_.tick();
    if (autoRefreshSeconds_ > 0 && !session_.syncRunning() &&
        std::chrono::steady_clock::now() - lastRefresh_ >= std::chrono::seconds(autoRefreshSeconds_)) {
      lastRefresh_ = std::chrono::steady_clock::now();
      session_.refresh();
    }
    drawFrame();
    if (auto key = terminal.readKey()) handleKey(*key);  // KEY_RESIZE: следующий кадр просто нарисуется в новом размере
  }
}

void App::drawFrame() {
  int h = 0, w = 0;
  getmaxyx(stdscr, h, w);
  erase();
  if (h < 5 || w < 30) {
    putText(0, 0, "Окно слишком маленькое", w);
  } else {
    views_.at(session_.mode())->draw(session_, Rect{0, 0, h - 1, w});
    drawStatusBar(session_, prompt_ ? std::optional<PromptView>(prompt_->view) : std::nullopt, h - 1, w);
  }
  refresh();
}

void App::handleKey(const Key& key) {
  if (prompt_) {
    handlePromptKey(key);
    return;
  }
  const std::string name = keyName(key);
  if (name.empty()) return;
  const auto commandName = keys_.commandFor(name);
  if (!commandName) return;
  const ICommand* command = commands_.find(*commandName);
  if (!command) return;
  if (auto p = command->prompt(session_)) {
    prompt_ = PromptState{command, PromptView{p->label, p->initial}};
  } else {
    runCommand(*command, session_, "");
  }
}

void App::handlePromptKey(const Key& key) {
  PromptState& p = *prompt_;
  const std::string name = keyName(key);
  if (name == "Esc") {
    prompt_.reset();
  } else if (name == "Enter") {
    const ICommand* command = p.command;
    const std::string text = p.view.text;
    prompt_.reset();
    runCommand(*command, session_, text);
  } else if (name == "Backspace") {
    std::string& t = p.view.text;
    while (!t.empty() && (static_cast<unsigned char>(t.back()) & 0xC0) == 0x80) t.pop_back();  // хвост UTF-8 символа
    if (!t.empty()) t.pop_back();
  } else {
    p.view.text += keyText(key);
  }
}

}  // namespace mail
