// The Settings page: graphics (the interface scale among them), how the board
// is shown, and making .sgf files open in this program.
//
// Built the way Factorio's OtherSettingsGui is: bordered frames under a
// subheader, each a setting's name with its control pushed to the right, or a
// group of check boxes under a caption; a red reset button in the subheader.
// Hovering the reset button lights up (as if hovered) every control it would
// put back to its default, and hovering Back every one changed since the page
// opened. Pressing reset only changes the page, and like any other change
// here that is only kept by Confirm.
//
// It edits a Settings in place, and the App applies each change as it is
// made, so a new scale or display mode can be seen before it is kept. Confirm
// keeps them; Back (or Esc) has the App put back what there was when the page
// opened. The file association is not a setting: its button asks the App to
// do it there and then.

#pragma once

#include <app/Settings.hpp>

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/Window.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace agui {
class Button;
class CheckBox;
class DropDown;
class Frame;
class ImageWidget;
class Label;
class LabelStyle;
class MouseEvent;
class Slider;
class TextField;
class VerticalFlow;
class Widget;
}  // namespace agui

namespace ui {

class GoSprites;
class Theme;

class SettingsPage : public agui::GenericTargetable {
public:
  SettingsPage(Theme& theme, const GoSprites& sprites, Settings& settings, std::function<void()> onAssociate,
               std::function<void()> onConfirm, std::function<void()> onBack);

  agui::Window& root() { return this->window; }

  // The page is being opened: what the settings are now is what Back would
  // go back to.
  void open();

  // Brings every control in line with the settings: when the page opens,
  // when changes are thrown away, and when the interface scale's keyboard
  // shortcut changes it from outside.
  void refresh();

  // What the line under the association button says.
  void setAssociation(const std::string& text, bool good);

private:
  // A bordered frame at the end of `content`, with a caption if given.
  agui::Frame& section(agui::VerticalFlow& content, const char* caption = nullptr);
  // A setting's name, followed by an info icon when it has a tooltip.
  agui::Widget& name(const char* text, const char* tip, const agui::LabelStyle* style = nullptr);
  agui::ImageWidget& info(const char* tip);
  // A row of `section`: the setting's name, and its control at the right end.
  void settingRow(agui::Frame& section, const char* name, const char* tip, agui::Widget& control);
  // A check box goes straight into its section, being its own name.
  void checkRow(agui::Frame& section, agui::CheckBox& box, const char* tip);

  // A setting as the reset and Back buttons see it: the control to light up,
  // and whether the setting differs from what it is in `other`.
  struct Setting {
    agui::Widget*                          widget;
    std::function<bool(const Settings&)> differs;
  };
  void track(agui::Widget& widget, std::function<bool(const Settings&)> differs);

  // While the mouse is on `source`, the control of every setting that
  // differs from `reference` is told the mouse is on it too, which is how
  // Factorio lights them up. Only `source` leaving puts them back, so the
  // mouse going from one button straight to the other keeps the second's.
  void highlight(const agui::Widget* source, const Settings& reference, const agui::MouseEvent& event);
  void unhighlight(const agui::Widget* source, const agui::MouseEvent& event);

  // After any change: the reset button is only there to press when there is
  // something to reset, and says how much.
  void changed();

  // The value typed into the tooltip delay's field, if it can be read.
  void typedTooltipDelay();

  Settings&        settings;
  Settings         openedWith;  // what Back goes back to
  Theme&           theme;
  const GoSprites& sprites;
  agui::Window     window;

  std::vector<int> fpsLimits;  // what each of the drop-down's items means
  agui::DropDown*  mode              = nullptr;
  agui::CheckBox*  vsync             = nullptr;
  agui::DropDown*  fps               = nullptr;
  agui::DropDown*  scale             = nullptr;
  agui::Slider*    tooltipDelay      = nullptr;
  agui::TextField* tooltipDelayValue = nullptr;
  agui::Label*     association       = nullptr;
  agui::Button*    reset             = nullptr;
  agui::ImageWidget* resetIcon       = nullptr;

  std::vector<Setting>       tracked;
  std::vector<agui::Widget*> lit;  // what highlight() lit up
  const agui::Widget*        litBy = nullptr;

  // The board's check boxes, and the setting each stands for.
  std::vector<std::pair<agui::CheckBox*, bool Settings::Board::*>> boardChecks;
};

}  // namespace ui
