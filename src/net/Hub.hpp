// The middle of a session: everyone's messages go to the hub, and it sends
// them on in one order, the same for all -- so when two people change the game
// at once, everyone ends up with the same game: the one the hub saw last.
//
// It keeps the latest game and position for whoever joins next, and who is
// in the session. It knows nothing of Go: the game is SGF text to it.
//
// The editor that hosts a session runs one, with itself in it as a member
// reached by a function call rather than a connection; the relay runs one
// for each room, with every member connected.

#pragma once

#include <net/Protocol.hpp>
#include <net/WebSocket.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace net {

class Hub {
public:
  // `room` is what joiners give to come in -- a code on the relay, "" for a
  // session hosted directly.
  explicit Hub(std::string room);

  // A member in the same program, told what happens through `deliver`. Returns
  // its id, which say() takes.
  uint32_t addLocal(std::string name, std::string identity, std::function<void(std::string_view message)> deliver);
  // What the local member says, as it would say it over a connection.
  void say(uint32_t member, std::string_view message);

  // Someone connected, whose Hello asked for this room with this name and
  // identity. They are welcomed with the game as it is, and everyone told
  // they are here.
  uint32_t admit(std::unique_ptr<WebSocket> connection, std::string name, std::string identity);

  // Reads what the connected members have said, and passes it on. True if
  // anything happened.
  bool poll();

  const std::string& room() const { return this->name; }
  bool               empty() const { return this->members.empty(); }
  size_t             size() const { return this->members.size(); }
  // Member `id`, or null if they aren't here.
  const Participant* who(uint32_t id) const;
  // What is said to everyone here, from nobody in particular.
  void               tell(std::string_view message) { this->broadcast(message); }

  // Messages the hub doesn't deal with itself -- Reachable -- go here, with
  // who said them: to the relay, which does.
  std::function<void(uint32_t from, std::string_view message)> onOther;

private:
  struct Member {
    Participant                                  who;
    std::unique_ptr<WebSocket>                   connection;  // null for the local member
    std::function<void(std::string_view)>        deliver;
  };

  uint32_t add(std::string name, std::string identity, std::unique_ptr<WebSocket> connection,
               std::function<void(std::string_view)> deliver);
  void     route(uint32_t from, std::string_view message);
  void     send(Member& to, std::string_view message);
  void     broadcast(std::string_view message, uint32_t except = 0);
  void     announce();
  Member*  find(uint32_t id);

  std::string              name;
  std::vector<Member>      members;
  uint32_t                 nextId = 1;
  std::optional<GameState> state;  // as the last State or Navigate left it
};

}  // namespace net
