// The New game page: the board's size, the handicap, komi and the players.
// It edits a GameSetup in place -- the settings keep it, so the next new
// game starts from the last one -- and says when to start.

#pragma once

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/Window.hpp>

#include <functional>

struct GameSetup;

namespace agui {
class DropDown;
class Label;
class Slider;
class TextField;
}  // namespace agui

namespace ui {

class Theme;

class NewGamePage : public agui::GenericTargetable {
public:
  NewGamePage(Theme& theme, GameSetup& setup, std::function<void()> onStart, std::function<void()> onBack);

  agui::Window& root() { return this->window; }

  // Pushes the setup out to the widgets, for when the page opens.
  void refresh();

private:
  void applyPreset(int index);
  void applySliders();

  GameSetup&   setup;
  agui::Window window;

  agui::DropDown*  size        = nullptr;
  agui::Slider*    width       = nullptr;
  agui::Slider*    height      = nullptr;
  agui::Slider*    handicap    = nullptr;
  agui::Label*     widthValue  = nullptr;
  agui::Label*     heightValue = nullptr;
  agui::Label*     handicapValue = nullptr;
  agui::TextField* komi        = nullptr;
  agui::TextField* black       = nullptr;
  agui::TextField* white       = nullptr;
};

}  // namespace ui
