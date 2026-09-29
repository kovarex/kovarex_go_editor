// The relay: a server anyone's editor can reach, for sharing a game when
// neither side can take connections -- behind a router, say. Run it where
// there is an address everyone can connect to.
//
// It keeps rooms, each a net::Hub, as an editor hosting a session does, but
// with every member connected. Joining with no room code opens a new room, and
// the Welcome tells its code, to be given to the others; joining with a code
// goes into that room. A room left empty is kept a while, game and all, so
// whoever lost their connection can come back to it.
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
#include <random>
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

// Room codes: easy to read out and type -- no 0/O or 1/I.
constexpr char   CODE_LETTERS[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
constexpr size_t CODE_LENGTH    = 6;

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
        const std::string code    = Normalized(in.str());
        if (!in.ok()) {
          w.connection->close();
        } else if (version != net::PROTOCOL_VERSION) {
          this->refuse(w, "The relay runs another version of the editor's sharing: update the editor.");
        } else {
          this->enter(w, name, code);
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

  void enter(Waiting& w, const std::string& name, std::string code)
  {
    if (code.empty()) {
      if (this->rooms.size() >= MAX_ROOMS) {
        this->refuse(w, "The relay is full. Try again later.");
        return;
      }
      do {
        code.clear();
        for (size_t i = 0; i < CODE_LENGTH; ++i) code += CODE_LETTERS[this->random() % (sizeof(CODE_LETTERS) - 1)];
      } while (this->rooms.contains(code));
      this->rooms[code].hub = std::make_unique<net::Hub>(code);
      Log("room " + code + " opened by " + w.peer);
    }
    const auto it = this->rooms.find(code);
    if (it == this->rooms.end()) {
      this->refuse(w, "There is no room " + code + " on the relay.");
      return;
    }
    if (it->second.hub->size() >= MAX_MEMBERS) {
      this->refuse(w, "Room " + code + " is full.");
      return;
    }
    it->second.hub->admit(std::move(w.connection), name);
    Log("room " + code + ": " + w.peer + " came in, " + std::to_string(it->second.hub->size()) + " there");
  }

  std::unique_ptr<net::Listener> listener;
  std::vector<Waiting>           waiting;
  std::map<std::string, Room>    rooms;
  std::mt19937_64                random{ std::random_device{}() };
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
