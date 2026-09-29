#include <app/Sharing.hpp>

#include <game/Game.hpp>
#include <game/Sgf.hpp>

#include <algorithm>

bool Sharing::host(int port, const std::string& name, const std::string& identity, std::string* error)
{
  this->leave();
  this->session = net::Session::host(port, name, identity, error);
  if (this->session) this->reachable = net::LocalAddresses();
  return this->session != nullptr;
}

void Sharing::join(const std::string& address, const std::string& room, const std::string& name, const std::string& identity)
{
  this->leave();
  this->session = net::Session::join(address, room, name, identity);
  this->relay   = address;
}

void Sharing::leave()
{
  this->session.reset();
  this->shared            = nullptr;
  this->welcomed          = false;
  this->unackedStates     = 0;
  this->unackedNavigation = 0;
  this->foreignState      = false;
  this->foreignNavigation = false;
  this->known.clear();
  this->paired.clear();
  this->strokes.clear();
}

void Sharing::update(Game& game)
{
  if (!this->session) return;
  this->session->update();
  // Only once welcomed: until then the game here isn't the session's, and
  // must not replace it.
  if (this->welcomed) this->publish(game);
  this->receive(game);
}

void Sharing::remember(const Game& game)
{
  this->shared   = &game;
  this->revision = game.revision();
  this->sgf      = sgf::Write(game.file());
  this->path     = game.path();
}

void Sharing::publish(Game& game)
{
  // Nothing happened here since the last look: nothing to say. (A game opened
  // or started anew is another Game, and always something to say.)
  if (&game == this->shared && game.revision() == this->revision) return;
  const bool             sameGame = &game == this->shared;
  const std::string      text     = sgf::Write(game.file());
  const std::vector<int> where    = game.path();
  this->shared   = &game;
  this->revision = game.revision();

  if (!sameGame || text != this->sgf) {
    this->session->sendState({ text, where });
    ++this->unackedStates;
  } else if (where != this->path) {
    this->session->sendNavigate(where);
    ++this->unackedNavigation;
  }
  this->sgf  = text;
  this->path = where;
}

void Sharing::apply(Game& game, const net::GameState& state)
{
  if (state.sgf == sgf::Write(game.file())) {
    // The same game: only where everyone is in it, if even that changed.
    if (state.path != game.path()) game.goToPath(state.path);
  } else if (std::optional<sgf::Collection> parsed = sgf::Parse(state.sgf)) {
    game.replace(std::move(*parsed), state.path);
  }
  this->remember(game);
}

void Sharing::receive(Game& game)
{
  using Kind = net::Session::Event::Kind;
  const uint32_t self = this->session->self();

  for (net::Session::Event& e : this->session->takeEvents()) {
    switch (e.kind) {
    case Kind::Joined:
      this->welcomed = true;
      this->known = this->session->participants();
      if (this->session->isHost()) {
        this->messages.push_back("Hosting: others can join now.");
      } else {
        // The game as the session has it replaces the one here. A room just
        // opened on the relay has none yet: it gets this one.
        if (e.state) {
          this->apply(game, *e.state);
          this->messages.push_back("Joined the session.");
        } else if (const Contact* with = this->pairRoom()) {
          this->messages.push_back("In your room with " + with->name + ", waiting for them.");
        } else if (!this->session->room().empty()) {
          this->messages.push_back("Opened room " + this->session->room() + " on the relay: give the others its code.");
        } else {
          this->messages.push_back("Joined the session.");
        }
      }
      this->pairUp();
      break;

    case Kind::State:
      if (!e.state) break;
      if (e.author == self) {
        // An edit of ours back from the hub. Already here -- unless someone
        // else's came in between, when the hub's last word is this one.
        this->unackedStates = std::max(0, this->unackedStates - 1);
        if (this->unackedStates == 0) {
          if (this->foreignState) this->apply(game, *e.state);
          this->foreignState = false;
        }
      } else {
        this->apply(game, *e.state);
        if (this->unackedStates > 0) this->foreignState = true;
      }
      break;

    case Kind::Navigate:
      if (e.author == self) {
        this->unackedNavigation = std::max(0, this->unackedNavigation - 1);
        if (this->unackedNavigation == 0) {
          if (this->foreignNavigation) {
            game.goToPath(e.path);
            this->remember(game);
          }
          this->foreignNavigation = false;
        }
      } else {
        game.goToPath(e.path);
        this->remember(game);
        if (this->unackedNavigation > 0) this->foreignNavigation = true;
      }
      break;

    case Kind::Stroke:
      this->strokes.push_back(std::move(e));
      break;

    case Kind::Participants: {
      const std::vector<net::Participant>& now = this->session->participants();
      for (const net::Participant& p : now) {
        const bool seen = std::any_of(this->known.begin(), this->known.end(), [&p](const auto& k) { return k.id == p.id; });
        if (!seen && p.id != self) this->messages.push_back(p.name + " joined.");
      }
      for (const net::Participant& k : this->known) {
        const bool here = std::any_of(now.begin(), now.end(), [&k](const auto& p) { return p.id == k.id; });
        if (!here) this->messages.push_back(k.name + " left.");
      }
      this->known = now;
      this->pairUp();
      break;
    }

    case Kind::Pair:
      // Someone here was here first, and gave the key of a room of the two
      // of them.
      for (const net::Participant& p : this->session->participants()) {
        if (p.id == e.author && !e.message.empty()) this->keep(p, e.message);
      }
      break;

    case Kind::Ended:
      this->messages.push_back(e.message.empty() ? std::string("The session ended.") : e.message);
      this->leave();
      return;
    }
  }
}

void Sharing::sendStroke(const net::StrokePart& part)
{
  if (this->joined()) this->session->sendStroke(part);
}

std::vector<net::Session::Event> Sharing::takeStrokes()
{
  std::vector<net::Session::Event> taken;
  taken.swap(this->strokes);
  return taken;
}

std::vector<std::string> Sharing::takeMessages()
{
  std::vector<std::string> taken;
  taken.swap(this->messages);
  return taken;
}

uint8_t Sharing::colourOf(uint32_t id) const
{
  if (!this->session) return 0;
  for (const net::Participant& p : this->session->participants()) {
    if (p.id == id) return p.colour;
  }
  return 0;
}

uint8_t Sharing::ownColour() const
{
  return this->session ? this->colourOf(this->session->self()) : 0;
}

const Contact* Sharing::pairRoom() const
{
  if (!this->contacts || !this->session || this->session->room().empty()) return nullptr;
  for (const Contact& c : *this->contacts) {
    if (c.key == this->session->room() && c.relay == this->relay) return &c;
  }
  return nullptr;
}

void Sharing::pairUp()
{
  // Only on a relay, where a room can be had again: an editor hosting has
  // an address, not rooms.
  if (!this->contacts || this->session->room().empty()) return;
  const uint32_t self = this->session->self();
  for (const net::Participant& p : this->session->participants()) {
    if (p.id == self || p.identity.empty()) continue;
    if (std::find(this->paired.begin(), this->paired.end(), p.id) != this->paired.end()) continue;
    this->paired.push_back(p.id);
    // Of the two, the one who was here first -- the lower id -- gives the
    // key: the one they already share, if there is one, or a new one.
    if (self > p.id) continue;
    const auto met = std::find_if(this->contacts->begin(), this->contacts->end(), [&](const Contact& c) {
      return c.identity == p.identity && c.relay == this->relay;
    });
    const std::string key = met != this->contacts->end() ? met->key : net::RandomCode(net::PAIR_KEY_LENGTH);
    this->session->sendPair(p.id, key);
    this->keep(p, key);
  }
}

void Sharing::keep(const net::Participant& who, const std::string& key)
{
  if (!this->contacts) return;
  const auto met = std::find_if(this->contacts->begin(), this->contacts->end(), [&](const Contact& c) {
    return c.identity == who.identity && c.relay == this->relay;
  });
  if (met == this->contacts->end()) {
    this->contacts->push_back({ who.identity, who.name, this->relay, key });
    this->messages.push_back(who.name + " is remembered: next time, pick them on the Online page.");
  } else if (met->name == who.name && met->key == key) {
    return;
  } else {
    met->name = who.name;
    met->key  = key;
  }
  this->changedContacts = true;
}
