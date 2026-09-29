// The relay: a server anyone's editor can reach, for sharing a game when
// neither side can take connections -- behind a router, say. Run it where
// there is an address everyone can connect to.
//
// It keeps rooms, each a net::Hub, as an editor hosting a session does, but
// with every member connected. Joining with no room code opens a new room, and
// the Welcome tells its code, to be given to the others; joining with a code
// goes into that room. Two people who have met keep a longer code of their own
// (see net::Type::Pair), and that room is made by whichever of them comes to
// it first. A room left empty is kept a while, game and all, so whoever lost
// their connection can come back to it.
//
//   go_relay [port]      (27272 by default)
//
// It speaks WebSocket, plain ws://, so it also works behind a web server
// that passes WebSocket connections through to it.

#include <net/Hub.hpp>
#include <net/Protocol.hpp>
#include <net/Socket.hpp>
#include <net/WebSocket.hpp>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// Enough for anyone using it honestly; a bound for anyone who isn't.
constexpr size_t MAX_ROOMS   = 1000;
constexpr size_t MAX_MEMBERS = 32;
constexpr size_t MAX_WAITING = 200;
// How long a connection has to say who it is.
constexpr auto HELLO_TIME = std::chrono::seconds(15);
// How long a room nobody is in is kept.
constexpr auto EMPTY_ROOM_TIME = std::chrono::minutes(30);
// Between looks while things happen, and while nothing does: quick when busy,
// and hardly any work at all when not.
constexpr auto BUSY_SLEEP = std::chrono::milliseconds(2);
constexpr auto IDLE_SLEEP = std::chrono::milliseconds(40);
constexpr auto BUSY_TIME  = std::chrono::seconds(2);
// Wrong room codes one address may try before it is turned away for a while:
// codes are too many to guess, and more so at this pace.
constexpr int  MAX_MISSES  = 10;
constexpr auto MISS_WINDOW = std::chrono::minutes(10);

void Log(const std::string& text)
{
  const std::time_t now = std::time(nullptr);
  char              when[32];
  std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", std::gmtime(&now));
  std::printf("%s %s\n", when, text.c_str());
  std::fflush(stdout);
}

struct Room {
  std::unique_ptr<net::Hub> hub;
  Clock::time_point         emptySince = Clock::now();
};

struct Waiting {
  std::unique_ptr<net::WebSocket> connection;
  std::string                     peer;
  Clock::time_point               since = Clock::now();
};

class Relay {
public:
  explicit Relay(std::unique_ptr<net::Listener> listener)
      : listener(std::move(listener))
  {}

  // One look at everything. True if anything happened.
  bool update()
  {
    bool busy = false;
    while (std::unique_ptr<net::Socket> socket = this->listener->accept()) {
      busy = true;
      if (this->waiting.size() >= MAX_WAITING) continue;  // dropped: closed as it goes
      const std::string peer = socket->peer();
      this->waiting.push_back({ net::WebSocket::server(std::move(socket)), peer });
    }

    const Clock::time_point now = Clock::now();
    for (size_t i = 0; i < this->waiting.size();) {
      Waiting&                 w = this->waiting[i];
      std::vector<std::string> messages;
      const bool               alive = w.connection->poll(messages);
      bool                     done  = !alive || now - w.since > HELLO_TIME;
      for (const std::string& message : messages) {
        busy = true;
        net::Reader in(message);
        if (in.type() != net::Type::Hello) continue;
        const uint32_t    version = in.u32();
        const std::string name    = in.str();
        const std::string code     = Normalized(in.str());
        const std::string identity = in.str();
        if (!in.ok()) {
          w.connection->close();
        } else if (version != net::PROTOCOL_VERSION) {
          this->refuse(w, "The relay runs another version of the editor's sharing: update the editor.");
        } else {
          this->enter(w, name, identity, code);
        }
        done = true;
        break;
      }
      if (done) this->waiting.erase(this->waiting.begin() + std::ptrdiff_t(i));
      else      ++i;
    }

    for (auto it = this->rooms.begin(); it != this->rooms.end();) {
      Room& room = it->second;
      if (room.hub->poll()) busy = true;
      if (!room.hub->empty()) {
        room.emptySince = now;
      } else if (now - room.emptySince > EMPTY_ROOM_TIME) {
        Log("room " + it->first + " closed");
        it = this->rooms.erase(it);
        continue;
      }
      ++it;
    }
    // Addresses that stopped missing are forgotten, now and then.
    if (this->misses.size() > 1000) {
      std::erase_if(this->misses, [now](const auto& m) { return now - m.second.since > MISS_WINDOW; });
    }
    return busy;
  }

private:
  // Codes are case-blind, and forgiving of spaces and dashes typed with them.
  static std::string Normalized(const std::string& code)
  {
    std::string out;
    for (const char c : code) {
      if (c >= 'a' && c <= 'z') out += char(c - 'a' + 'A');
      else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) out += c;
    }
    return out;
  }

  void refuse(Waiting& w, const std::string& why)
  {
    w.connection->send(net::Writer(net::Type::Refused).str(why).bytes());
    w.connection->close();
  }

  void enter(Waiting& w, const std::string& name, const std::string& identity, std::string code)
  {
    // Someone trying code after code is guessing: turned away for a while.
    const std::string       address = w.peer.substr(0, w.peer.rfind(':'));
    const Clock::time_point now     = Clock::now();
    Misses&                 misses  = this->misses[address];
    if (now - misses.since > MISS_WINDOW) misses = { 0, now };
    if (misses.count >= MAX_MISSES) {
      this->refuse(w, "Too many wrong room codes. Try again in a few minutes.");
      return;
    }

    const bool pair = code.size() == net::PAIR_KEY_LENGTH;
    if (code.empty() || (pair && !this->rooms.contains(code))) {
      if (this->rooms.size() >= MAX_ROOMS) {
        this->refuse(w, "The relay is full. Try again later.");
        return;
      }
      if (code.empty()) {
        do code = net::RandomCode(net::ROOM_CODE_LENGTH);
        while (this->rooms.contains(code));
      }
      this->rooms[code].hub = std::make_unique<net::Hub>(code);
      Log("room " + Shown(code) + " opened by " + w.peer);
    }
    const auto it = this->rooms.find(code);
    if (it == this->rooms.end()) {
      ++misses.count;
      this->refuse(w, "There is no room " + code + " on the relay.");
      return;
    }
    if (it->second.hub->size() >= MAX_MEMBERS) {
      this->refuse(w, "The room is full.");
      return;
    }
    it->second.hub->admit(std::move(w.connection), name, identity);
    Log("room " + Shown(code) + ": " + w.peer + " came in, " + std::to_string(it->second.hub->size()) + " there");
  }

  // A pair's code is theirs alone, and stays out of the log.
  static std::string Shown(const std::string& code)
  {
    return code.size() == net::PAIR_KEY_LENGTH ? code.substr(0, 4) + "... (a pair's)" : code;
  }

  struct Misses {
    int               count = 0;
    Clock::time_point since = Clock::now();
  };

  std::unique_ptr<net::Listener> listener;
  std::vector<Waiting>           waiting;
  std::map<std::string, Room>    rooms;
  std::map<std::string, Misses>  misses;  // by address
};

}  // namespace

int main(int argc, char** argv)
{
#ifdef SIGPIPE
  std::signal(SIGPIPE, SIG_IGN);  // a connection gone is an error to handle, not the end of the relay
#endif
  const int port = argc > 1 ? std::atoi(argv[1]) : net::DEFAULT_PORT;
  if (port <= 0 || port >= 65536) {
    std::fprintf(stderr, "usage: %s [port]\n", argv[0]);
    return 2;
  }
  std::string                    error;
  std::unique_ptr<net::Listener> listener = net::Listener::open(port, &error);
  if (!listener) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  Log("listening on port " + std::to_string(port));

  Relay             relay(std::move(listener));
  Clock::time_point lastBusy = Clock::now();
  for (;;) {
    if (relay.update()) lastBusy = Clock::now();
    std::this_thread::sleep_for(Clock::now() - lastBusy < BUSY_TIME ? BUSY_SLEEP : IDLE_SLEEP);
  }
}
