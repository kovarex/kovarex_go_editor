// The Game info page: the root node's game information -- players, ranks,
// result, date, event, rules and the rest SGF has room for -- as fields to
// edit. Applied all at once, as one undo step.

#pragma once

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/Window.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace sgf {
class Node;
}

namespace agui {
class TextBox;
class TextField;
}  // namespace agui

namespace ui {

class Theme;

class GameInfoPage : public agui::GenericTargetable {
public:
  GameInfoPage(Theme& theme, std::function<void()> onApply, std::function<void()> onBack);

  agui::Window& root() { return this->window; }

  // Fills the fields from a game's root.
  void load(const sgf::Node& root);

  // Every property the page edits, with what its field says now. Empty
  // values mean "take the property out".
  std::vector<std::pair<std::string, std::string>> values() const;

private:
  agui::Window window;

  std::vector<std::pair<std::string, agui::TextField*>> fields;
  agui::TextBox* gameComment = nullptr;
};

}  // namespace ui
