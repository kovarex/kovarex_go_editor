#include <ui/SettingsPage.hpp>

#include <app/Settings.hpp>
#include <ui/Form.hpp>
#include <ui/GoSprites.hpp>
#include <ui/Theme.hpp>

#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/CheckBox.hpp>
#include <Agui/Widget/DropDown.hpp>
#include <Agui/Widget/Frame.hpp>
#include <Agui/Widget/HorizontalFlow.hpp>
#include <Agui/Widget/ImageWidget.hpp>
#include <Agui/Widget/Label.hpp>
#include <Agui/Widget/Slider.hpp>
#include <Agui/Widget/VerticalFlow.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace ui {

namespace {

// What Reset puts back: the settings a first run starts with.
const Settings DEFAULTS{};

// The reset button, a tool button, and the icon on it.
constexpr int BUTTON_PX = 34;
constexpr int ICON_PX   = 24;

std::string Percent(int value)
{
  return std::to_string(value) + "%";
}

}  // namespace

SettingsPage::SettingsPage(Theme& theme, const GoSprites& sprites, Settings& settings, std::function<void()> onAssociate,
                           std::function<void()> onSave, std::function<void()> onBack)
    : settings(settings)
    , theme(theme)
    , window(agui::GuiDirection::Vertical, "Settings")
{
  this->window.setDragTarget(&this->window);
  using Graphics = Settings::Graphics;

  agui::VerticalFlow& content = column(0);
  content.style.setPaddings(8, 12, 12, 12);

  // --- graphics ---
  content << agui::label("Graphics", &theme.headingLabel);

  this->mode = &make<agui::DropDown>();
  this->mode->addItems({ "Windowed", "Windowed (fullscreen)" });
  this->mode->setItemToolTip(1, "A borderless window over the whole monitor. The monitor keeps its mode, "
                                "and alt-tab and other windows work as usual.");
  this->mode->onItemSelect(this, [this](int i) {
    this->settings.graphics.windowedFullscreen = i == 1;
    this->changed();
  });
  content << this->settingRow("Display mode", *this->mode, [this] {
    return this->settings.graphics.windowedFullscreen != DEFAULTS.graphics.windowedFullscreen;
  });

  this->vsync = &make<agui::CheckBox>();
  this->vsync->setToolTip("Wait for the monitor between frames: no tearing, and no more frames than it can show.");
  this->vsync->onCheckChange(this, [this](bool on) {
    this->settings.graphics.vsync = on;
    this->changed();
  });
  content << this->settingRow("VSync", *this->vsync, [this] { return this->settings.graphics.vsync != DEFAULTS.graphics.vsync; });

  this->fpsLimits = { 0, 30, 60, 120, 144, 240 };
  // A hand-edited limit that isn't one of the choices gets an item of its own.
  if (std::find(this->fpsLimits.begin(), this->fpsLimits.end(), settings.graphics.fpsLimit) == this->fpsLimits.end()) {
    this->fpsLimits.push_back(settings.graphics.fpsLimit);
  }
  this->fps = &make<agui::DropDown>();
  for (int limit : this->fpsLimits) this->fps->addItem(limit ? std::to_string(limit) : std::string("None"));
  this->fps->onItemSelect(this, [this](int i) {
    this->settings.graphics.fpsLimit = this->fpsLimits[size_t(i)];
    this->changed();
  });
  content << this->settingRow("Frame rate limit", *this->fps,
                              [this] { return this->settings.graphics.fpsLimit != DEFAULTS.graphics.fpsLimit; });

  this->scale = &make<agui::DropDown>();
  for (int percent = Graphics::MIN_SCALE; percent <= Graphics::MAX_SCALE; percent += Graphics::SCALE_STEP) {
    this->scale->addItem(Percent(percent));
  }
  this->scale->setToolTip("How big everything is drawn. Ctrl and numpad + or - change it from anywhere.");
  this->scale->onItemSelect(this, [this](int i) {
    this->settings.graphics.interfaceScale = Graphics::MIN_SCALE + i * Graphics::SCALE_STEP;
    this->changed();
  });
  content << this->settingRow("Interface scale", *this->scale,
                              [this] { return this->settings.graphics.interfaceScale != DEFAULTS.graphics.interfaceScale; });

  // 0 to 200 ms, and one notch past the end for "never".
  this->tooltipDelay = &make<agui::Slider>();
  this->tooltipDelay->setMinMaxValues(0, Graphics::MAX_TOOLTIP_DELAY + Graphics::TOOLTIP_DELAY_STEP);
  this->tooltipDelay->setValueStep(Graphics::TOOLTIP_DELAY_STEP);
  this->tooltipDelay->style.setMinimalWidth(200);
  this->tooltipDelay->setToolTip("How long the mouse rests on something before its tooltip shows. "
                                 "Hold Shift to see tooltips at once, whatever this says.");
  this->tooltipDelayValue = &agui::label("");
  this->tooltipDelayValue->style.setMinimalWidth(80);
  this->tooltipDelay->onSliderMove(this, [this](double v) {
    const int ms = int(std::lround(v));
    this->settings.graphics.tooltipDelay = ms > Graphics::MAX_TOOLTIP_DELAY ? Graphics::TOOLTIPS_NEVER : ms;
    this->refresh();
  });
  agui::HorizontalFlow& delay = row();
  delay << *this->tooltipDelay << *this->tooltipDelayValue;
  content << this->settingRow("Tooltip delay", delay,
                              [this] { return this->settings.graphics.tooltipDelay != DEFAULTS.graphics.tooltipDelay; });

  // --- the board ---
  content << agui::label("Board", &theme.headingLabel);

  const auto check = [this, &content](const char* name, bool& value, bool fallback, const char* tip) {
    agui::CheckBox& box = make<agui::CheckBox>();
    box.setToolTip(tip);
    box.onCheckChange(this, [this, &value](bool on) {
      value = on;
      this->changed();
    });
    content << this->settingRow(name, box, [&value, fallback] { return value != fallback; });
    this->boardChecks.emplace_back(&box, &value);
  };
  check("Coordinates", settings.board.coordinates, DEFAULTS.board.coordinates,
        "Letters and numbers round the edge of the board.");
  check("Move numbers", settings.board.moveNumbers, DEFAULTS.board.moveNumbers,
        "The move number on every stone, rather than a dot on the last one.");
  check("Next moves", settings.board.nextMoves, DEFAULTS.board.nextMoves,
        "Where the variations from the current move go, as faint stones.");

  // --- files: an action, not a setting, so Reset leaves it be ---
  content << agui::label("Files", &theme.headingLabel);
  agui::Frame& files = make<agui::Frame>(agui::GuiDirection::Horizontal, &theme.settingRow);
  files << namedRow("Open .sgf files", agui::button("Open them with this program", &this->window, std::move(onAssociate),
                                                   &theme.smallButton));
  content << files;
  this->association = &agui::label("", &theme.dimLabel, agui::SingleLine::False);
  this->association->style.setMaximalWidth(440);
  this->association->style.setLeftPadding(6);
  content << *this->association;

  // Factorio's layout: a deep panel with a subheader strip across its top,
  // and the reset button -- green, a circling arrow -- at the strip's right.
  // Hovering it lights up every setting it would change; pressing it only
  // changes the page, and Save changes is still what keeps anything.
  this->reset = &make<agui::Button>(&theme.greenToolButton);
  this->reset->setFocusable(false);
  this->reset->setToolTip("Reset to defaults. The settings it would change light up while the mouse is here; "
                          "nothing is kept until Save changes.");
  this->resetIcon = &make<agui::ImageWidget>(sprites.image(Sprite::Reset));
  this->resetIcon->scaleToKeepTheRatio = true;
  this->resetIcon->setIgnoredByInteraction(true);
  this->resetIcon->style.setMinimalWidth(ICON_PX);
  this->resetIcon->style.setMaximalWidth(ICON_PX);
  this->resetIcon->style.setMinimalHeight(ICON_PX);
  this->resetIcon->style.setMaximalHeight(ICON_PX);
  *this->reset << *this->resetIcon;
  this->reset->onMouseEnter(this, [this](const agui::MouseEvent&) {
    this->hoveringReset = true;
    this->highlight();
  });
  this->reset->onMouseLeave(this, [this](const agui::MouseEvent&) {
    this->hoveringReset = false;
    this->highlight();
  });
  this->reset->onClick(this, [this] {
    this->settings.graphics = DEFAULTS.graphics;
    this->settings.board    = DEFAULTS.board;
    this->refresh();
  });

  agui::HorizontalFlow& strip = row();
  strip.style.setHorizontallyStretchable(true);
  strip << agui::pusher << *this->reset;
  agui::Frame& subheader = make<agui::Frame>(agui::GuiDirection::Horizontal, &theme.subheaderFrame);
  subheader << strip;

  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &theme.insideDeepFrame);
  panel << subheader << content;
  this->window << panel;

  // dialog_buttons_horizontal_flow: Back throws the changes away, the green
  // one keeps them.
  agui::HorizontalFlow& footer = row(8);
  footer.style.setTopPadding(8);
  footer.style.setHorizontallyStretchable(true);
  footer << agui::button("Back", &this->window, std::move(onBack), &theme.backButton);
  footer << dragHandle(&theme.draggableSpace, &this->window);
  footer << footerButton("Save changes", &this->window, std::move(onSave), &theme.forwardButton, 200);
  this->window << footer;

  this->refresh();
}

agui::Frame& SettingsPage::settingRow(const char* name, agui::Widget& control, std::function<bool()> differs)
{
  agui::Frame& frame = make<agui::Frame>(agui::GuiDirection::Horizontal, &this->theme.settingRow);
  frame << namedRow(name, control);
  this->rows.push_back({ &frame, std::move(differs) });
  return frame;
}

void SettingsPage::refresh()
{
  using Graphics = Settings::Graphics;
  const Graphics& g = this->settings.graphics;
  this->mode->setSelectedIndex(g.windowedFullscreen ? 1 : 0);
  if (this->vsync->isChecked() != g.vsync) this->vsync->setChecked(g.vsync);
  const auto limit = std::find(this->fpsLimits.begin(), this->fpsLimits.end(), g.fpsLimit);
  if (limit != this->fpsLimits.end()) this->fps->setSelectedIndex(int(limit - this->fpsLimits.begin()));
  for (const auto& [box, value] : this->boardChecks) {
    if (box->isChecked() != *value) box->setChecked(*value);
  }

  this->scale->setSelectedIndex((g.interfaceScale - Graphics::MIN_SCALE) / Graphics::SCALE_STEP);

  const int  delay = g.tooltipDelay;
  const bool never = delay == Graphics::TOOLTIPS_NEVER;
  this->tooltipDelay->setValue(never ? Graphics::MAX_TOOLTIP_DELAY + Graphics::TOOLTIP_DELAY_STEP : delay);
  this->tooltipDelayValue->setText(never ? std::string("Never") : delay == 0 ? std::string("Instant") : std::to_string(delay) + " ms");

  // The icon in the middle of the button. A button's children sit inside its
  // padding, border and all, so that comes off.
  this->resetIcon->setLocation((BUTTON_PX - ICON_PX) / 2 - this->reset->getLeftPadding(),
                               (BUTTON_PX - ICON_PX) / 2 - this->reset->getTopPadding());
  this->changed();
}

void SettingsPage::changed()
{
  // Nothing to reset when everything is already the default.
  const bool any = std::any_of(this->rows.begin(), this->rows.end(), [](const Row& r) { return r.differs(); });
  if (this->reset->isEnabled() != any) this->reset->setEnabled(any);
  // A disabled button hears no more of the mouse, not even it leaving.
  if (!any) this->hoveringReset = false;
  this->highlight();
}

void SettingsPage::highlight()
{
  for (const Row& r : this->rows) {
    const agui::FrameStyle* look = this->hoveringReset && r.differs() ? &this->theme.settingRowChanged : &this->theme.settingRow;
    if (r.frame->style.getParent() != look) r.frame->style.setParent(look);
  }
}

void SettingsPage::setAssociation(const std::string& text, bool good)
{
  this->association->style.setParent(good ? &this->theme.goodLabel : &this->theme.badLabel);
  this->association->setText(text);
}

}  // namespace ui
