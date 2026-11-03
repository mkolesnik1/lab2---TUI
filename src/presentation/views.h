#pragma once
#include <optional>
#include <string>

#include "core/application/commands.h"
#include "core/application/key_bindings.h"
#include "core/application/session.h"

namespace mail {

struct Rect {
  int y, x, h, w;
};

// Strategy: каждый режим сеанса (Mode) рисует свой экран. Новый экран = новый класс + регистрация в App.
class IView {
 public:
  virtual ~IView() = default;
  virtual void draw(MailboxSession& session, const Rect& area) = 0;
};

class MailListView final : public IView {
 public:
  void draw(MailboxSession& session, const Rect& area) override;

 private:
  size_t top_ = 0;  // первая видимая строка (чисто «оконное» состояние представления)
};

class MessageView final : public IView {
 public:
  void draw(MailboxSession& session, const Rect& area) override;

 private:
  size_t partsTop_ = 0;
};

class HelpView final : public IView {
 public:
  HelpView(const CommandRegistry& commands, const KeyBindings& keys) : commands_(commands), keys_(keys) {}
  void draw(MailboxSession& session, const Rect& area) override;

 private:
  const CommandRegistry& commands_;
  const KeyBindings& keys_;
};

// Ввод аргумента команды (поиск, путь для сохранения).
struct PromptView {
  std::string label;
  std::string text;
};

// Строка состояния: режим, сортировка, позиция, сообщения и ошибки.
void drawStatusBar(const MailboxSession& session, const std::optional<PromptView>& prompt, int y, int width);

}  // namespace mail
