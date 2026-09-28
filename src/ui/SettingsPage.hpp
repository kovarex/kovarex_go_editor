// The Settings page: graphics (the interface scale among them), how the board
// is shown, and making .sgf files open in this program.
//
// Laid out like Factorio's: a subheader strip across the top of the panel
// with a green reset button. Hovering it lights up every setting it would
// put back to its default; pressing it does so on the page, and like any
// other change here that is only kept by Save changes.
//
// It edits a Settings in place, and the App applies each change as it is
// made, so a new scale or display mode can be seen before it is kept. Save
// changes keeps them; Back (or Esc) has the App put back what there was when
// the page opened. The file association is not a setting: its button asks
// the App to do it there and then.

#pragma once

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/Window.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

struct Settings;

namespace agui {
class Button;
class CheckBox;
class Frame;
class ImageWidget;
class Widget;
class DropDown;
class Label;
class Slider;
}  // namespace agui

namespace ui {

class GoSprites;
class Theme;

class SettingsPage : public agui::GenericTargetable {
public:
  SettingsPage(Theme& theme, const GoSprites& sprites, Settings& settings, std::function<void()> onAssociate,
               std::function<void()> onSave, std::function<void()> onBack);

  agui::Window& root() { return this->window; }

  // Brings every control in line with the settings: when the page opens,
  // when changes are thrown away, and when the interface scale's keyboard
  // shortcut changes it from outside.
  void refresh();

  // What the line under the association button says.
  void setAssociation(const std::string& text, bool good);

private:
  // A setting's name and control in a row that can be lit up, and whether
  // the setting is off its default -- what the reset button would change.
  agui::Frame& settingRow(const char* name, agui::Widget& control, std::function<bool()> differs);
  // After any change: the reset button is only there to press when there is
  // something to reset, and the lit rows follow the settings.
  void changed();
  void highlight();

  struct Row {
    agui::Frame*          frame;
    std::function<bool()> differs;
  };

  Settings&    settings;
  Theme&       theme;
  agui::Window window;

  std::vector<int> fpsLimits;  // what each of the drop-down's items means
  agui::DropDown*  mode        = nullptr;
  agui::CheckBox*  vsync       = nullptr;
  agui::DropDown*  fps         = nullptr;
  agui::DropDown*  scale       = nullptr;
  agui::Slider*    tooltipDelay      = nullptr;
  agui::Label*     tooltipDelayValue = nullptr;
  agui::Label*     association = nullptr;
  agui::Button*    reset       = nullptr;
  agui::ImageWidget* resetIcon = nullptr;
  bool             hoveringReset = false;
  std::vector<Row> rows;

  // The board's check boxes, and the setting each stands for.
  std::vector<std::pair<agui::CheckBox*, bool*>> boardChecks;
};

}  // namespace ui
