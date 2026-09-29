
// The Host and Join pages: sharing the game with other editors, so that
// everyone can edit it and everyone sees the same position -- for teaching,
// mostly. One class, in one of two modes.
//
// Both are made for the relay first. Host starts a session there, whose
// invite code brings in someone new; or, without the relay, has this editor
// host on a port. Join offers the people met before, to join them without a
// code; a session by its invite code; or an editor that hosts, at its
// address. Both end with the relay's address. With a session, either shows
// who is in it, each in the colour their drawing on the board comes out in,
// the invite code to pass on, and a way to leave.
//
// It edits the OnlineSetup the settings keep, so the name and the addresses
// are there next time; the rest is the App's to do.

#pragma once

#include <net/Protocol.hpp>

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/Window.hpp>

#include <functional>
#include <string>
#include <vector>

struct OnlineSetup;

namespace agui {
class Button;
class Frame;
class Label;
class TextField;
class VerticalFlow;
}  // namespace agui

namespace ui {

class Theme;

class OnlinePage : public agui::GenericTargetable {
public:
  // What the player asked for.
  enum class Request {
    Connect,     // join contact `index`: in their session, or in the room the two of you have
    Forget,      // forget contact `index`
    OpenRoom,    // a new session, in a new room on setup.relay
    JoinRoom,    // the session whose invite code is setup.room, on setup.relay
    Host,        // host a session on setup.port, without the relay
    JoinDirect,  // join the editor hosting at setup.address
    Leave,       // leave the session, or stop hosting it
  };

  // Hosting a session, or joining one: two pages of their own, with what
  // they share -- the name, the relay's address, and the session once in it.
  enum class Mode { Host, Join };

  OnlinePage(Theme& theme, OnlineSetup& setup, Mode mode, std::function<void(Request, size_t index)> onRequest,
             std::function<void()> onBack);

  agui::Window& root() { return this->window; }

  // The setup out to the fields and the people met, for when the page opens
  // or they change.
  void refresh();

  // The session as it is: none, being joined, or in -- and who is in it.
  struct Status {
    bool                          active = false;  // hosting, joined, or joining
    std::string                   what;            // a line saying which, and where
    std::string                   code;            // the invite code to pass on; empty if there is none
    std::vector<net::Participant> people;
    uint32_t                      self = 0;
    bool operator==(const Status&) const = default;
  };
  // Cheap when nothing changed: called every frame while the page is up.
  void show(const Status& status);

private:
  // Lists the people met, if they changed since; true if they did.
  bool listContacts();

  Theme&       theme;
  OnlineSetup& setup;
  agui::Window window;

  std::function<void(Request, size_t)> onRequest;

  agui::TextField*    name    = nullptr;
  agui::TextField*    code    = nullptr;
  agui::TextField*    relay   = nullptr;
  agui::TextField*    port    = nullptr;
  agui::TextField*    address = nullptr;
  agui::Frame*        contactSection = nullptr;
  agui::VerticalFlow* contactList    = nullptr;
  std::vector<agui::Frame*> sections;  // shown when not in a session
  agui::Frame*        sessionSection = nullptr;
  agui::Label*        state    = nullptr;
  agui::Widget*       codeRow  = nullptr;  // the invite code, and Copy
  agui::Label*        codeText = nullptr;
  agui::Button*       copy     = nullptr;
  agui::VerticalFlow* people   = nullptr;

  Status                   shown;
  bool                     everShown = false;
  std::vector<std::string> listed;  // the contacts' names as the list shows them
};

}  // namespace ui
