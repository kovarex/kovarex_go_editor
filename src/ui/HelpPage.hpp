// The Help page: every keyboard shortcut and what the mouse does on the
// board, read off the same tables that make them work, so it can't go out
// of date.

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

class HelpPage : public agui::GenericTargetable {
public:
  HelpPage(Theme& theme, std::function<void()> onBack);

  agui::Window& root() { return this->window; }

private:
  agui::Window window;
};

// "Save changes to game.sgf?" -- before a new game, another file or quitting
// throws unsaved changes away.
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
