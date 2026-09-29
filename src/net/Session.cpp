#include <net/Session.hpp>

#include <cstdlib>
#include <thread>

namespace net {

// A connection being made on a thread of its own: what it came to, handed
// over under the lock. Shared, so a session given up on can go while the
// thread is still waiting on a slow address.
struct Session::Connecting {
  std::mutex              lock;
  bool                    done = false;
  std::unique_ptr<Socket> socket;
  std::string             error;
};

namespace {

// "host", "host:port", "[v6]:port", or "ws://host[:port]/path".
bool ParseAddress(std::string address, std::string& host, int& port, std::string& path)
{
  path = "/";
  port = DEFAULT_PORT;
  if (address.starts_with("ws://")) {
    address = address.substr(5);
    port    = 80;
    if (const size_t slash = address.find('/'); slash != std::string::npos) {
      path    = address.substr(slash);
      address = address.substr(0, slash);
    }
  }
  if (address.starts_with("[")) {  // an IPv6 address, bracketed so its colons aren't a port
    const size_t close = address.find(']');
    if (close == std::string::npos) return false;
    host = address.substr(1, close - 1);
    if (close + 1 < address.size() && address[close + 1] == ':') port = std::atoi(address.c_str() + close + 2);
  } else if (const size_t colon = address.rfind(':'); colon != std::string::npos && address.find(':') == colon) {
    host = address.substr(0, colon);
    port = std::atoi(address.c_str() + colon + 1);
  } else {
    host = address;
  }
  return !host.empty() && port > 0 && port < 65536;
}

}  // namespace

std::unique_ptr<Session> Session::host(int port, std::string name, std::string* error)
{
  std::unique_ptr<Listener> listener = Listener::open(port, error);
  if (!listener) return nullptr;
  std::unique_ptr<Session> session(new Session());
  session->listener   = std::move(listener);
  session->listenPort = port;
  session->hub        = std::make_unique<Hub>("");
  Session* self       = session.get();
  session->selfId     = session->hub->addLocal(std::move(name), [self](std::string_view message) { self->handle(message); });
  return session;
}

std::unique_ptr<Session> Session::join(const std::string& address, const std::string& room, std::string name)
{
  std::unique_ptr<Session> session(new Session());
  int port = 0;
  if (!ParseAddress(address, session->joinHost, port, session->joinPath)) {
    session->end("That isn't an address: " + address);
    return session;
  }
  session->joinRoom   = room;
  session->joinName   = std::move(name);
  session->connecting = std::make_shared<Connecting>();
  std::thread([pending = session->connecting, host = session->joinHost, port] {
    std::string             error;
    std::unique_ptr<Socket> socket = Socket::connect(host, port, &error);
    std::lock_guard         guard(pending->lock);
    pending->socket = std::move(socket);
    pending->error  = error;
    pending->done   = true;
  }).detach();
  return session;
}

Session::~Session()
{
  if (this->connection) this->connection->close();
}

void Session::update()
{
  if (this->ended) return;

  if (this->hub) {
    // Newcomers: their Hello says who they are and which version they speak.
    while (std::unique_ptr<Socket> socket = this->listener->accept()) {
      this->arriving.push_back({ WebSocket::server(std::move(socket)) });
    }
    for (size_t i = 0; i < this->arriving.size();) {
      Arrival&                 a = this->arriving[i];
      std::vector<std::string> messages;
      const bool               alive = a.connection->poll(messages);
      bool                     done  = !alive;
      for (const std::string& message : messages) {
        Reader in(message);
        if (in.type() != Type::Hello) continue;
        const uint32_t    version = in.u32();
        const std::string who     = in.str();
        in.str();  // the room: there is only this one
        if (version != PROTOCOL_VERSION) {
          a.connection->send(Writer(Type::Refused).str("This session is run by another version of the editor.").bytes());
          a.connection->close();
        } else {
          this->hub->admit(std::move(a.connection), who);
        }
        done = true;
        break;
      }
      if (done) this->arriving.erase(this->arriving.begin() + std::ptrdiff_t(i));
      else      ++i;
    }
    this->hub->poll();
    return;
  }

  if (this->connecting) {
    // What the thread came to, taken under its lock -- and the shared state
    // let go of only once the lock is, since that may be the last of it.
    std::unique_ptr<Socket> socket;
    std::string             error;
    {
      std::lock_guard guard(this->connecting->lock);
      if (!this->connecting->done) return;
      socket = std::move(this->connecting->socket);
      error  = this->connecting->error;
    }
    this->connecting.reset();
    if (!socket) {
      this->end(error);
      return;
    }
    this->connection = WebSocket::client(std::move(socket), this->joinHost, this->joinPath);
    this->connection->send(Writer(Type::Hello).u32(PROTOCOL_VERSION).str(this->joinName).str(this->joinRoom).bytes());
  }
  if (this->connection) {
    std::vector<std::string> messages;
    const bool               alive = this->connection->poll(messages);
    for (const std::string& message : messages) this->handle(message);
    if (!alive && !this->ended) {
      this->end(this->selfId ? "The session ended: the other side went away."
                             : this->connection->error().empty() ? "Couldn't join the session."
                                                                 : this->connection->error());
    }
  }
}

void Session::handle(std::string_view message)
{
  Reader in(message);
  Event  e;
  switch (in.type()) {
  case Type::Welcome: {
    this->selfId   = in.u32();
    this->roomCode = in.str();
    this->people.clear();
    const uint32_t n = in.u32();
    for (uint32_t i = 0; i < n && in.ok(); ++i) this->people.push_back(in.participant());
    if (in.u8()) e.state = in.state();
    e.kind = Event::Kind::Joined;
    break;
  }
  case Type::Participants: {
    this->people.clear();
    const uint32_t n = in.u32();
    for (uint32_t i = 0; i < n && in.ok(); ++i) this->people.push_back(in.participant());
    e.kind = Event::Kind::Participants;
    break;
  }
  case Type::State:
    e.kind   = Event::Kind::State;
    e.author = in.u32();
    e.state  = in.state();
    break;
  case Type::Navigate:
    e.kind   = Event::Kind::Navigate;
    e.author = in.u32();
    e.path   = in.path();
    break;
  case Type::Stroke: {
    e.kind          = Event::Kind::Stroke;
    e.author        = in.u32();
    e.stroke.stroke = in.u32();
    e.stroke.finished = in.u8() != 0;
    const uint32_t n = in.u32();
    for (uint32_t i = 0; i < n && in.ok(); ++i) {
      const float x = in.f32();
      e.stroke.points.emplace_back(x, in.f32());
    }
    break;
  }
  case Type::Refused:
    this->end(in.str());
    return;
  default:
    return;
  }
  if (in.ok()) this->events.push_back(std::move(e));
}

std::vector<Session::Event> Session::takeEvents()
{
  std::vector<Event> taken;
  taken.swap(this->events);
  return taken;
}

void Session::send(std::string message)
{
  if (this->hub)             this->hub->say(this->selfId, message);
  else if (this->connection) this->connection->send(message);
}

void Session::sendState(const GameState& state)
{
  this->send(Writer(Type::State).state(state).bytes());
}

void Session::sendNavigate(const std::vector<int>& path)
{
  this->send(Writer(Type::Navigate).path(path).bytes());
}

void Session::sendStroke(const StrokePart& stroke)
{
  Writer w(Type::Stroke);
  w.u32(stroke.stroke).u8(stroke.finished ? 1 : 0).u32(uint32_t(stroke.points.size()));
  for (const auto& [x, y] : stroke.points) w.f32(x).f32(y);
  this->send(w.bytes());
}

void Session::end(std::string why)
{
  if (this->ended) return;
  this->ended = true;
  Event e;
  e.kind    = Event::Kind::Ended;
  e.message = std::move(why);
  this->events.push_back(std::move(e));
}

}  // namespace net
