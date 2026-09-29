#include <net/Hub.hpp>

#include <algorithm>

namespace net {

namespace {

// Colours are an index into the palette every editor has; this many of them.
constexpr uint8_t COLOURS = 8;

// Names are shown on screen, and a hostile client could send anything.
std::string Clean(std::string name)
{
  if (name.size() > 32) name.resize(32);
  std::erase_if(name, [](char c) { return static_cast<unsigned char>(c) < 32; });
  return name.empty() ? std::string("Someone") : name;
}

// Identities are made of code letters; anything else is dropped.
std::string CleanIdentity(std::string identity)
{
  if (identity.size() > 32) identity.resize(32);
  std::erase_if(identity, [](char c) { return CODE_LETTERS.find(c) == std::string_view::npos; });
  return identity;
}

}  // namespace

Hub::Hub(std::string room)
    : name(std::move(room))
{}

uint32_t Hub::add(std::string who, std::string identity, std::unique_ptr<WebSocket> connection,
                  std::function<void(std::string_view)> deliver)
{
  // The first colour nobody here has.
  uint8_t colour = 0;
  while (colour < COLOURS && std::any_of(this->members.begin(), this->members.end(),
                                         [colour](const Member& m) { return m.who.colour == colour; })) {
    ++colour;
  }
  Member m;
  m.who        = { this->nextId++, Clean(std::move(who)), uint8_t(colour % COLOURS), CleanIdentity(std::move(identity)) };
  m.connection = std::move(connection);
  m.deliver    = std::move(deliver);
  this->members.push_back(std::move(m));
  Member& added = this->members.back();

  Writer welcome(Type::Welcome);
  welcome.u32(added.who.id).str(this->name).u32(uint32_t(this->members.size()));
  for (const Member& each : this->members) welcome.participant(each.who);
  welcome.u8(this->state ? 1 : 0);
  if (this->state) welcome.state(*this->state);
  this->send(added, welcome.bytes());
  this->announce();
  return added.who.id;
}

uint32_t Hub::addLocal(std::string who, std::string identity, std::function<void(std::string_view)> deliver)
{
  return this->add(std::move(who), std::move(identity), nullptr, std::move(deliver));
}

void Hub::admit(std::unique_ptr<WebSocket> connection, std::string who, std::string identity)
{
  this->add(std::move(who), std::move(identity), std::move(connection), nullptr);
}

void Hub::say(uint32_t member, std::string_view message)
{
  this->route(member, message);
}

bool Hub::poll()
{
  bool someoneLeft = false, busy = false;
  for (size_t i = 0; i < this->members.size(); ++i) {
    Member& m = this->members[i];
    if (!m.connection) continue;
    std::vector<std::string> messages;
    const bool alive = m.connection->poll(messages);
    const uint32_t id = m.who.id;
    if (!messages.empty()) busy = true;
    for (const std::string& message : messages) this->route(id, message);
    if (!alive) {
      // route() may have added to members; find them again rather than trust `m`.
      std::erase_if(this->members, [id](const Member& each) { return each.who.id == id; });
      someoneLeft = true;
      --i;
    }
  }
  if (someoneLeft) this->announce();
  return busy || someoneLeft;
}

Hub::Member* Hub::find(uint32_t id)
{
  const auto it = std::find_if(this->members.begin(), this->members.end(), [id](const Member& m) { return m.who.id == id; });
  return it == this->members.end() ? nullptr : &*it;
}

void Hub::route(uint32_t from, std::string_view message)
{
  Reader in(message);
  switch (in.type()) {
  case Type::State: {
    GameState s = in.state();
    if (!in.ok()) return;
    this->state = s;
    // To everyone, whoever sent it included: the order the hub puts things in
    // is the one everybody follows, their own edits among the rest.
    this->broadcast(Writer(Type::State).u32(from).state(s).bytes());
    break;
  }
  case Type::Navigate: {
    std::vector<int> path = in.path();
    if (!in.ok()) return;
    if (this->state) this->state->path = path;
    this->broadcast(Writer(Type::Navigate).u32(from).path(path).bytes());
    break;
  }
  case Type::Stroke: {
    // Drawn lines aren't part of the game, and the one drawing sees theirs
    // already: only the others are told.
    std::string relayed = Writer(Type::Stroke).u32(from).bytes();
    relayed.append(message.substr(1));
    this->broadcast(relayed, from);
    break;
  }
  case Type::Pair: {
    // For one member only: a room key two people keep between them.
    const uint32_t    to  = in.u32();
    const std::string key = in.str();
    if (!in.ok() || to == from) return;
    if (Member* target = this->find(to)) this->send(*target, Writer(Type::Pair).u32(from).str(key).bytes());
    break;
  }
  default:
    break;
  }
}

void Hub::send(Member& to, std::string_view message)
{
  if (to.connection) to.connection->send(message);
  else if (to.deliver) to.deliver(message);
}

void Hub::broadcast(std::string_view message, uint32_t except)
{
  for (Member& m : this->members) {
    if (m.who.id != except) this->send(m, message);
  }
}

void Hub::announce()
{
  Writer list(Type::Participants);
  list.u32(uint32_t(this->members.size()));
  for (const Member& m : this->members) list.participant(m.who);
  this->broadcast(list.bytes());
}

}  // namespace net
