#include <ui/OnlinePage.hpp>

#include <app/Settings.hpp>
#include <ui/BoardView.hpp>
#include <ui/Form.hpp>
#include <ui/Theme.hpp>

#include <Agui/Graphics.hpp>
#include <Agui/PaintEvent.hpp>
#include <Agui/SystemClipboard.hpp>
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

OnlinePage::OnlinePage(Theme& theme, OnlineSetup& setup, std::function<void(Request, size_t)> onRequest,
                       std::function<void()> onBack)
    : theme(theme)
    , setup(setup)
    , window(agui::GuiDirection::Vertical, "Online")
    , onRequest(std::move(onRequest))
{
  this->window.setDragTarget(&this->window);
  agui::VerticalFlow& content = column(8);
  const auto ask = [this](Request request) { return [this, request] { this->onRequest(request, 0); }; };

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

  // The people met before: one click to meet again.
  this->contactSection = &section("People you met");
  this->contactList    = &column(4);
  *this->contactSection << *this->contactList
                        << note("Connect to join them, no code needed: in their session if they are in one, or they "
                                "come to you when they connect to you. Nobody else can come in that way.");
  this->contactSection->setVisible(false);
  content << *this->contactSection;

  // A session on the relay: a new one, or one someone invited this editor to.
  this->roomSection = &section("New people");
  agui::HorizontalFlow& openRow = row(8);
  openRow.style.setHorizontallyStretchable(true);
  openRow << agui::label("A new session, with the game here") << agui::pusher
          << agui::button("Start", &this->window, ask(Request::OpenRoom));
  this->code = &Field(120);
  this->code->setToolTip("The invite code someone gave you.");
  this->code->onTextEdit(this, [this] { this->setup.room = this->code->getText(); });
  agui::HorizontalFlow& joinRow = namedRow("Invite code", *this->code);
  joinRow << agui::pusher << agui::button("Join", &this->window, ask(Request::JoinRoom));
  *this->roomSection << openRow << joinRow
                     << note("For someone you haven't met yet: start a session and give them its invite code. Joining "
                             "someone's session brings its game here, in place of this one.");
  content << *this->roomSection;

  // Everything else: another relay, or none -- this editor hosting, or
  // joining one that does, which needs a port let through a router.
  this->otherSection = &section("Other ways to connect");
  this->relay = &Field();
  this->relay->setToolTip(std::string("The relay the rooms are on: ") + net::DEFAULT_RELAY + " unless you run your own.");
  this->relay->onTextEdit(this, [this] { this->setup.relay = this->relay->getText(); });
  this->port = &Field(80);
  this->port->setToolTip("The port others connect to, when this editor hosts.");
  this->port->onTextEdit(this, [this] {
    const int value = std::atoi(this->port->getText().c_str());
    if (value > 0 && value < 65536) this->setup.port = value;
  });
  agui::HorizontalFlow& hostRow = namedRow("Host on port", *this->port);
  hostRow << agui::pusher << agui::button("Host", &this->window, ask(Request::Host));
  this->address = &Field(180);
  this->address->setToolTip("The address of an editor that hosts, as its player tells you -- with the port after a "
                            "colon if it isn't the usual one.");
  this->address->onTextEdit(this, [this] { this->setup.address = this->address->getText(); });
  agui::HorizontalFlow& directRow = namedRow("Join an editor at", *this->address);
  directRow << agui::pusher << agui::button("Join", &this->window, ask(Request::JoinDirect));
  *this->otherSection << namedRow("Relay", *this->relay) << hostRow << directRow
                      << note("Without the relay, one editor hosts and the others connect to its address. From outside "
                              "its network, its router has to let the port through.");
  content << *this->otherSection;

  // In one.
  this->sessionSection = &section("In a session");
  this->state = &agui::label("", nullptr, agui::SingleLine::False);
  this->state->style.setMaximalWidth(NOTE_W);
  agui::HorizontalFlow& codeLine = row(8);
  codeLine.style.setHorizontallyStretchable(true);
  this->codeText = &agui::label("", &theme.headingLabel);
  this->copy     = &agui::button("Copy code", &this->window, [this] {
    agui::SystemClipboard::copy(this->shown.code);
    this->copy->setText(std::string("Copied"));
  });
  codeLine << agui::label("Invite code") << *this->codeText << agui::pusher << *this->copy;
  this->codeRow = &codeLine;
  this->people  = &column(4);
  agui::HorizontalFlow& leaveRow = row(8);
  leaveRow.style.setHorizontallyStretchable(true);
  leaveRow << agui::pusher << agui::button("Leave", &this->window, ask(Request::Leave));
  *this->sessionSection << *this->state << codeLine << *this->people << leaveRow;
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
  this->code->setText(this->setup.room);
  this->relay->setText(this->setup.relay);
  this->port->setText(std::to_string(this->setup.port));
  this->address->setText(this->setup.address);
  this->listContacts();
}

bool OnlinePage::listContacts()
{
  std::vector<std::string> names;
  for (const Contact& c : this->setup.contacts) names.push_back(c.name);
  if (names == this->listed) return false;
  this->listed = names;

  this->contactList->clear();
  for (size_t i = 0; i < names.size(); ++i) {
    agui::HorizontalFlow& line = row(8);
    line.style.setHorizontallyStretchable(true);
    agui::Label& who = agui::label(names[i]);
    who.setToolTip("On " + this->setup.contacts[i].relay);
    line << who << agui::pusher
         << agui::button("Connect", &this->window, [this, i] { this->onRequest(Request::Connect, i); })
         << agui::button("Forget", &this->window, [this, i] { this->onRequest(Request::Forget, i); });
    *this->contactList << line;
  }
  return true;
}

void OnlinePage::show(const Status& status)
{
  const bool listChanged = this->listContacts();
  if (this->everShown && !listChanged && status == this->shown) return;
  this->everShown = true;
  if (status.code != this->shown.code) this->copy->setText(std::string("Copy code"));
  this->shown = status;

  this->contactSection->setVisible(!status.active && !this->setup.contacts.empty());
  this->roomSection->setVisible(!status.active);
  this->otherSection->setVisible(!status.active);
  this->sessionSection->setVisible(status.active);
  this->state->setText(status.what);
  this->codeRow->setVisible(!status.code.empty());
  this->codeText->setText(status.code);

  this->people->clear();
  for (const net::Participant& p : status.people) {
    agui::HorizontalFlow& line = row(8);
    line << make<Swatch>(ParticipantColour(p.colour)) << agui::label(p.id == status.self ? p.name + "  (you)" : p.name);
    *this->people << line;
  }
}

}  // namespace ui
