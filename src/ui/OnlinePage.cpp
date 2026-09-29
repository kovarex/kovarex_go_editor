#include <ui/OnlinePage.hpp>

#include <app/Settings.hpp>
#include <ui/BoardView.hpp>
#include <ui/Form.hpp>
#include <ui/Theme.hpp>

#include <Agui/Graphics.hpp>
#include <Agui/PaintEvent.hpp>
#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/EmptyWidget.hpp>
#include <Agui/Widget/Frame.hpp>
#include <Agui/Widget/Label.hpp>
#include <Agui/Widget/TextField.hpp>

#include <cstdlib>

namespace ui {

namespace {

constexpr int FIELD_W = 260;
constexpr int NOTE_W  = 420;
// The square of a participant's colour before their name.
constexpr int SWATCH_PX = 12;

// A square of a participant's colour, before their name.
class Swatch : public agui::EmptyWidget {
public:
  explicit Swatch(agui::Color colour)
      : colour(colour)
  {
    this->setIgnoredByInteraction(true);
    this->style.setMinimalWidth(SWATCH_PX);
    this->style.setMaximalWidth(SWATCH_PX);
    this->style.setMinimalHeight(SWATCH_PX);
    this->style.setMaximalHeight(SWATCH_PX);
  }

protected:
  void paintComponent(const agui::PaintEvent& paintEvent, const agui::Point&) override
  {
    paintEvent.graphics()->drawFilledRectangle(agui::Rectangle(0, 0, this->getWidth(), this->getHeight()), this->colour);
  }

private:
  agui::Color colour;
};

agui::TextField& Field(int width = FIELD_W)
{
  agui::TextField& field = make<agui::TextField>();
  field.style.setMinimalWidth(width);
  field.style.setMaximalWidth(width);
  return field;
}

}  // namespace

OnlinePage::OnlinePage(Theme& theme, OnlineSetup& setup, std::function<void()> onHost, std::function<void()> onJoin,
                       std::function<void()> onLeave, std::function<void()> onBack)
    : theme(theme)
    , setup(setup)
    , window(agui::GuiDirection::Vertical, "Online")
{
  this->window.setDragTarget(&this->window);
  agui::VerticalFlow& content = column(8);

  this->name = &Field();
  this->name->setToolTip("What the others see you as.");
  this->name->onTextEdit(this, [this] { this->setup.name = this->name->getText(); });
  content << namedRow("Your name", *this->name);

  const auto section = [&theme](const char* caption) -> agui::Frame& {
    agui::Frame& frame = make<agui::Frame>(agui::GuiDirection::Vertical, &theme.borderedFrame);
    frame << agui::label(caption, &theme.captionLabel);
    return frame;
  };
  const auto note = [&theme](const char* text) -> agui::Label& {
    agui::Label& label = agui::label(text, &theme.dimLabel, agui::SingleLine::False);
    label.style.setMaximalWidth(NOTE_W);
    return label;
  };

  // Hosting: this editor runs the session, and the others connect to it.
  this->hostSection = &section("Host a session");
  this->port = &Field(80);
  this->port->onTextEdit(this, [this] {
    const int value = std::atoi(this->port->getText().c_str());
    if (value > 0 && value < 65536) this->setup.port = value;
  });
  agui::HorizontalFlow& hostRow = namedRow("Port", *this->port);
  hostRow << agui::pusher << agui::button("Host", &this->window, std::move(onHost));
  *this->hostSection << hostRow
                     << note("Everyone who joins edits the game with you, and sees the same position. They connect to "
                             "this computer's address -- the page shows it once you host. From outside your network, "
                             "your router has to let the port through to this computer.");
  content << *this->hostSection;

  // Joining someone else's.
  this->joinSection = &section("Join a session");
  this->address = &Field();
  this->address->setToolTip("The host's address, as they tell you -- with the port after a colon if it isn't the usual one.");
  this->address->onTextEdit(this, [this] { this->setup.address = this->address->getText(); });
  this->room = &Field(120);
  this->room->setToolTip("Only on a relay: the code of the room to join, or nothing to open a new room.");
  this->room->onTextEdit(this, [this] { this->setup.room = this->room->getText(); });
  agui::HorizontalFlow& joinRow = namedRow("Room", *this->room);
  joinRow << agui::pusher << agui::button("Join", &this->window, std::move(onJoin));
  *this->joinSection << namedRow("Address", *this->address) << joinRow
                     << note("The game here is replaced by the session's, as the host has it. On a relay, joining "
                             "with no room code opens a new room, with the game here in it, and shows its code to "
                             "give the others.");
  content << *this->joinSection;

  // In one.
  this->sessionSection = &section("In a session");
  this->state = &agui::label("", nullptr, agui::SingleLine::False);
  this->state->style.setMaximalWidth(NOTE_W);
  this->people = &column(4);
  agui::HorizontalFlow& leaveRow = row(8);
  leaveRow.style.setHorizontallyStretchable(true);
  leaveRow << agui::pusher << agui::button("Leave", &this->window, std::move(onLeave));
  *this->sessionSection << *this->state << *this->people << leaveRow;
  this->sessionSection->setVisible(false);
  content << *this->sessionSection;

  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &theme.insideShallowFrameWithPadding);
  panel << content;
  this->window << panel;

  agui::HorizontalFlow& footer = row(8);
  footer.style.setTopPadding(8);
  footer.style.setHorizontallyStretchable(true);
  footer << agui::button("Back", &this->window, std::move(onBack), &theme.backButton);
  footer << dragHandle(&theme.draggableSpace, &this->window);
  this->window << footer;
}

void OnlinePage::refresh()
{
  this->name->setText(this->setup.name);
  this->port->setText(std::to_string(this->setup.port));
  this->address->setText(this->setup.address);
  this->room->setText(this->setup.room);
}

void OnlinePage::show(const Status& status)
{
  const bool samePeople = status.people.size() == this->shown.people.size() &&
                          std::equal(status.people.begin(), status.people.end(), this->shown.people.begin(),
                                     [](const net::Participant& a, const net::Participant& b) {
                                       return a.id == b.id && a.name == b.name && a.colour == b.colour;
                                     });
  if (this->everShown && status.active == this->shown.active && status.what == this->shown.what && samePeople &&
      status.self == this->shown.self) {
    return;
  }
  this->everShown = true;
  this->shown     = status;

  this->hostSection->setVisible(!status.active);
  this->joinSection->setVisible(!status.active);
  this->sessionSection->setVisible(status.active);
  this->state->setText(status.what);

  this->people->clear();
  for (const net::Participant& p : status.people) {
    agui::HorizontalFlow& line = row(8);
    line << make<Swatch>(ParticipantColour(p.colour)) << agui::label(p.id == status.self ? p.name + "  (you)" : p.name);
    *this->people << line;
  }
}

}  // namespace ui
