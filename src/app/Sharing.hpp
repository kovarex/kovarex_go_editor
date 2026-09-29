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

#include <app/Settings.hpp>
#include <net/Session.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Game;

class Sharing {
public:
  bool active() const { return this->session != nullptr; }
  bool joined() const { return this->session && this->session->isJoined(); }
  bool hosting() const { return this->session && this->session->isHost(); }
  // Whether this editor started the session it is in on the relay (joined
  // with no code, so the relay opened a new room): its host, as far as the
  // player is concerned.
  bool started() const
  {
    return this->session && this->session->isJoined() && !this->session->room().empty() && this->joinedRoom.empty();
  }
  const net::Session* current() const { return this->session.get(); }
  // Where others can reach this editor while it hosts: looked up once, as
  // hosting starts.
  const std::vector<std::string>& addresses() const { return this->reachable; }
  // The address joined.
  const std::string& address() const { return this->relay; }

  // False with the reason in `error` if the port can't be had.
  bool host(int port, const std::string& name, const std::string& identity, std::string* error);
  void join(const std::string& address, const std::string& room, const std::string& name, const std::string& identity);
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

  // The people met on a relay, kept in `list`: added to when someone new is
  // met, and their names kept up to date. contactsChanged() says, once, that
  // the list should be saved.
  void setContacts(std::vector<Contact>* list) { this->contacts = list; }
  bool contactsChanged() { return std::exchange(this->changedContacts, false); }
  // The contact whose own room this session is in, if it is one.
  const Contact* pairRoom() const;

private:
  void publish(Game& game);
  void receive(Game& game);
  void apply(Game& game, const net::GameState& state);
  void remember(const Game& game);
  void pairUp();
  void keep(const net::Participant& who, const std::string& key);
  // Tells the relay the keys of the people met, so they find this editor.
  void sayReachable();

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

  std::vector<Contact>* contacts        = nullptr;
  bool                  changedContacts = false;
  std::string           relay;   // the address joined, which is a relay if the room has a code
  std::vector<uint32_t> paired;  // participants already paired with, this session
  std::string           joinedName, joinedIdentity, joinedRoom;  // as joined with, to join again
  bool                  announce = false;  // the people met should be told where this editor is
  bool                  rejoin   = false;  // the relay said to join again
};
