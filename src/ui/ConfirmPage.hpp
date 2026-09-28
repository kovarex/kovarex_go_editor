// "Save changes to game.sgf?" -- before a new game, another file or quitting
// throws unsaved changes away.

#pragma once

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/Window.hpp>

#include <functional>
#include <string>

namespace agui {
class Label;
}

namespace ui {

class Theme;

class ConfirmPage : public agui::GenericTargetable {
public:
  ConfirmPage(Theme& theme, std::function<void()> onSave, std::function<void()> onDiscard, std::function<void()> onBack);

  agui::Window& root() { return this->window; }

  void setFile(const std::string& name);

private:
  agui::Window window;
  agui::Label* question = nullptr;
};

}  // namespace ui
