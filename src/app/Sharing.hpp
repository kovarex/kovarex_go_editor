// Sharing the game being edited with other editors, over a net::Session:
// whatever anyone does to the record, or wherever they go in it, everyone
// else's editor does too.
//
// Each frame it sends what changed here -- the whole record as SGF, or just
// the position when only that moved -- and applies what the others did. Its
// own edits are applied here at once, and their echo from the hub is let
// pass; but when someone else's edit landed in between, the echo is applied
// after all, because the hub's order is the one everybody ends up with.

#pragma once

#include <net/Session.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Game;

class Sharing {
public:
  bool active() const { return this->session != nullptr; }
  bool joined() const { return this->session && this->session->isJoined(); }
  bool hosting() const { return this->session && this->session->isHost(); }
  const net::Session* current() const { return this->session.get(); }
  // Where others can reach this editor while it hosts: looked up once, as
  // hosting starts.
  const std::vector<std::string>& addresses() const { return this->reachable; }

  // False with the reason in `error` if the port can't be had.
  bool host(int port, const std::string& name, std::string* error);
  void join(const std::string& address, const std::string& room, const std::string& name);
  void leave();

  // Once a frame, after the editor has done this frame's edits: sends them,
  // then applies what the others did to `game`.
  void update(Game& game);

  // What was drawn here, for the others; and what they drew, for the board.
  void sendStroke(const net::StrokePart& part);
  std::vector<net::Session::Event> takeStrokes();

  // What to tell the player: someone joined or left, the session ended.
  std::vector<std::string> takeMessages();

  // The colour index of participant `id`, or of this editor when alone.
  uint8_t colourOf(uint32_t id) const;
  uint8_t ownColour() const;

private:
  void publish(Game& game);
  void receive(Game& game);
  void apply(Game& game, const net::GameState& state);
  void remember(const Game& game);

  std::unique_ptr<net::Session> session;
  bool                          welcomed = false;  // the Welcome read: in, with the session's game

  // What the others were last told, or last told this editor: sent again only
  // when the game differs from it.
  const Game*      shared   = nullptr;
  uint64_t         revision = 0;
  std::string      sgf;
  std::vector<int> path;

  // Own edits sent and not yet come back from the hub, and whether anyone
  // else's arrived meanwhile.
  int  unackedStates     = 0;
  int  unackedNavigation = 0;
  bool foreignState      = false;
  bool foreignNavigation = false;

  std::vector<std::string>          messages;
  std::vector<net::Session::Event>  strokes;
  std::vector<net::Participant>     known;  // who was here, to say who came and went
  std::vector<std::string>          reachable;
};
