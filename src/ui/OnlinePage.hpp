// The Online page: sharing the game with other editors, so that everyone can
// edit it and everyone sees the same position -- for teaching, mostly.
//
// Without a session it offers to host one (others then join this editor) or
// to join one; with one, who is in it, each in the colour their drawing on
// the board comes out in, and a way to leave. It edits the OnlineSetup the
// settings keep, so the name and the addresses are there next time; hosting,
// joining and leaving are the App's to do.

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
  OnlinePage(Theme& theme, OnlineSetup& setup, std::function<void()> onHost, std::function<void()> onJoin,
             std::function<void()> onLeave, std::function<void()> onBack);

  agui::Window& root() { return this->window; }

  // The setup out to the fields, for when the page opens.
  void refresh();

  // The session as it is: none, being joined, or in -- and who is in it.
  struct Status {
    bool                          active = false;  // hosting, joined, or joining
    std::string                   what;            // a line saying which, and where
    std::vector<net::Participant> people;
    uint32_t                      self = 0;
  };
  // Cheap when nothing changed: called every frame while the page is up.
  void show(const Status& status);

private:
  Theme&       theme;
  OnlineSetup& setup;
  agui::Window window;

  agui::TextField*    name    = nullptr;
  agui::TextField*    port    = nullptr;
  agui::TextField*    address = nullptr;
  agui::TextField*    room    = nullptr;
  agui::Frame*        hostSection    = nullptr;
  agui::Frame*        joinSection    = nullptr;
  agui::Frame*        sessionSection = nullptr;
  agui::Label*        state   = nullptr;
  agui::VerticalFlow* people  = nullptr;

  Status shown;
  bool   everShown = false;
};

}  // namespace ui
