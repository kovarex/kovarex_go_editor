// This editor's part in a shared session: hosting one -- listening for others
// and running the hub, with this editor as one of its members -- or joining
// one, hosted by another editor or on the relay.
//
// Either way it looks the same from outside: the editor says what it did
// (sendState, sendNavigate, sendStroke) and, each frame, takes what happened
// (takeEvents) -- the hub's order of everyone's edits, its own included.

#pragma once

#include <net/Hub.hpp>
#include <net/Protocol.hpp>
#include <net/Socket.hpp>
#include <net/WebSocket.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace net {

class Session {
public:
  // Hosts a session on `port`. Null with the reason in `error` if the port
  // can't be had.
  static std::unique_ptr<Session> host(int port, std::string name, std::string identity, std::string* error);
  // Joins the session at `address`: "host" or "host:port" for one hosted
  // directly, or a ws:// address for the relay, with the room's code. What
  // becomes of it arrives as events.
  static std::unique_ptr<Session> join(const std::string& address, const std::string& room, std::string name,
                                       std::string identity);

  ~Session();
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  struct Event {
    enum class Kind {
      Joined,        // in, as `self()`; `state`, if set, is the game as it is
      State,         // the game and the position, as `author` left them
      Navigate,      // the position only
      Stroke,        // `author` drew
      Participants,  // who is here changed
      Pair,          // `author` gave this editor a room key for the two of them: `message`
      Ended,         // the session is over; `message` says why
    };
    Kind                     kind = Kind::Ended;
    uint32_t                 author = 0;
    std::optional<GameState> state;
    std::vector<int>         path;
    StrokePart               stroke;
    std::string              message;
  };

  // Moves messages along. Once a frame.
  void update();
  // What happened since the last call, oldest first.
  std::vector<Event> takeEvents();

  void sendState(const GameState& state);
  void sendNavigate(const std::vector<int>& path);
  void sendStroke(const StrokePart& stroke);
  // To participant `to` only: a room key for the two of them.
  void sendPair(uint32_t to, const std::string& key);

  bool     isHost() const { return this->hub != nullptr; }
  bool     isJoined() const { return this->selfId != 0; }
  uint32_t self() const { return this->selfId; }
  const std::vector<Participant>& participants() const { return this->people; }
  // The room's code, on the relay.
  const std::string& room() const { return this->roomCode; }
  int port() const { return this->listenPort; }

private:
  Session() = default;

  void handle(std::string_view message);
  void send(std::string message);
  void end(std::string why);

  // Hosting.
  std::unique_ptr<Listener> listener;
  std::unique_ptr<Hub>      hub;
  struct Arrival {
    std::unique_ptr<WebSocket> connection;
  };
  std::vector<Arrival> arriving;  // connected, their Hello not yet read
  int                  listenPort = 0;

  // Joining.
  struct Connecting;
  std::shared_ptr<Connecting> connecting;  // shared with the thread doing it
  std::unique_ptr<WebSocket>  connection;
  std::string                 joinHost, joinPath, joinRoom, joinName, joinIdentity;

  uint32_t                 selfId = 0;
  std::string              roomCode;
  std::vector<Participant> people;
  std::vector<Event>       events;
  bool                     ended = false;
};

}  // namespace net
