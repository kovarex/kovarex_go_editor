#include <ui/HelpPage.hpp>

#include <ui/Form.hpp>
#include <ui/Shortcuts.hpp>
#include <ui/Theme.hpp>

#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/Frame.hpp>
#include <Agui/Widget/Label.hpp>
#include <Agui/Widget/Table.hpp>
#include <Agui/Widget/VerticalScrollPane.hpp>

namespace ui {

namespace {

// What the mouse does on the board, with the Play tool.
constexpr const char* MOUSE[][2] = {
  { "Click", "Play a move -- or step into it, if the record already has it" },
  { "Ctrl+click", "Insert the move after this one; the rest of the game follows on after it" },
  { "Drag a stone", "Move it, changing the move that played it, however long ago" },
  { "Right-click a stone", "Go to the move that played it (Shift+click too)" },
  { "Mouse wheel", "Back and forward through the game" },
  { "With a mark tool", "Click puts the mark on, or takes it off; right-click clears the point" },
  { "Arrow and line tools", "Click one end, then the other" },
  { "Hold Shift", "Show tooltips at once, whatever the tooltip delay in Settings says" },
};

}  // namespace

HelpPage::HelpPage(Theme& theme, std::function<void()> onBack)
    : window(agui::GuiDirection::Vertical, "Help")
{
  this->window.setDragTarget(&this->window);

  agui::VerticalScrollPane& scroll = make<agui::VerticalScrollPane>();
  scroll.style.setMinimalWidth(640);
  scroll.style.setMaximalHeight(520);

  const auto section = [&theme, &scroll](const char* heading) {
    scroll << agui::label(heading, &theme.headingLabel);
    agui::Table& table = make<agui::Table>(2u);
    table.style.setHorizontalSpacing(16);
    scroll << table;
    return &table;
  };

  agui::Table* mouse = section("The board");
  for (const auto& [what, does] : MOUSE) {
    agui::Label& key = agui::label(what, &theme.dimLabel);
    key.style.setMinimalWidth(172);
    *mouse << key << agui::label(does);
  }

  agui::Table* keys = section("Keys");
  for (const Shortcut& s : AllShortcuts()) {
    agui::Label& key = agui::label(s.keys, &theme.dimLabel);
    key.style.setMinimalWidth(172);
    *keys << key << agui::label(s.what);
  }
  agui::Label& note = agui::label("The keys work wherever the mouse or the focus is -- except in a text box, where only the "
                                  "Ctrl and F keys do. Esc, or a click on the board, leaves the text box.",
                                  &theme.dimLabel, agui::SingleLine::False);
  note.style.setMaximalWidth(620);
  scroll << note;

  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &theme.insideShallowFrameWithPadding);
  panel << scroll;
  this->window << panel;

  agui::HorizontalFlow& footer = row(8);
  footer.style.setTopPadding(8);
  footer.style.setHorizontallyStretchable(true);
  footer << agui::button("Back", &this->window, std::move(onBack), &theme.backButton);
  footer << dragHandle(&theme.draggableSpace, &this->window);
  this->window << footer;
}

ConfirmPage::ConfirmPage(Theme& theme, std::function<void()> onSave, std::function<void()> onDiscard,
                         std::function<void()> onBack)
    : window(agui::GuiDirection::Vertical, "Unsaved changes")
{
  this->window.setDragTarget(&this->window);

  this->question = &agui::label("");
  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &theme.insideShallowFrameWithPadding);
  panel << *this->question;
  this->window << panel;

  agui::HorizontalFlow& footer = row(8);
  footer.style.setTopPadding(8);
  footer.style.setHorizontallyStretchable(true);
  footer << agui::button("Cancel", &this->window, std::move(onBack), &theme.backButton);
  footer << dragHandle(&theme.draggableSpace, &this->window);
  footer << footerButton("Don't save", &this->window, std::move(onDiscard), &theme.redBackButton, 140);
  footer << footerButton("Save", &this->window, std::move(onSave), &theme.forwardButton, 140);
  this->window << footer;
}

void ConfirmPage::setFile(const std::string& name)
{
  this->question->setText("Save the changes to " + name + " first?");
}

}  // namespace ui
