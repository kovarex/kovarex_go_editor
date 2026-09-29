// What editors in a session say to each other, and how it is written down:
// each message is one WebSocket message, a type byte and then its fields --
// numbers little-endian, strings as their length and their bytes.
//
// The game travels as SGF text and the position as a path through its tree,
// so nothing here knows anything about Go, and the relay (which only passes
// messages on) doesn't either.

#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace net {

// Bumped whenever a message changes, so an old editor is told, rather than
// misreading a new one.
constexpr uint32_t PROTOCOL_VERSION = 3;

// The port a session listens on unless told otherwise: an editor hosting one,
// or the relay.
constexpr int DEFAULT_PORT = 27272;

// The relay the editor offers to join through until told otherwise: the
// project's own, on NearlyFreeSpeech (see relay/README.md).
constexpr const char* DEFAULT_RELAY = "ws://kovarexgoeditor.com";

// Room codes on the relay, in letters easy to read out and type -- no 0/O or
// 1/I. A code ROOM_CODE_LENGTH long is a room someone opened, which has to
// exist to be joined. One PAIR_KEY_LENGTH long is two people's own room (see
// Type::Pair), made by whichever of them comes first: too long to guess, so
// only the two of them can come in.
constexpr std::string_view CODE_LETTERS     = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
constexpr size_t           ROOM_CODE_LENGTH = 6;
constexpr size_t           PAIR_KEY_LENGTH  = 20;

// A code of `length` random CODE_LETTERS.
inline std::string RandomCode(size_t length)
{
  static std::mt19937_64 random{ std::random_device{}() };
  std::string code;
  for (size_t i = 0; i < length; ++i) code += CODE_LETTERS[random() % CODE_LETTERS.size()];
  return code;
}

enum class Type : uint8_t {
  Hello = 1,     // client -> hub: who I am, the room I want, and my identity
  Welcome,       // hub -> client: your id, the room, who is here, the game
  Participants,  // hub -> all: who is here now
  State,         // any -> hub -> all: the game and the position, after an edit
  Navigate,      // any -> hub -> all: the position only
  Stroke,        // any -> hub -> the others: points drawn on the board
  Refused,       // hub -> client: why you can't join
  Pair,          // client -> hub -> one other: the key of a room for the two of them
  Reachable,     // client -> relay: the pair keys whose other halves may come to where I am
  Moved,         // relay -> client: the other of your pair is in a session now; ask for the key again
};

struct Participant {
  uint32_t    id = 0;
  std::string name;
  uint8_t     colour = 0;  // an index into the palette every editor has
  // A random name each editor makes up for itself once, and keeps: how
  // editors know someone they met before. Anyone can claim anyone's, so it
  // recognises people but proves nothing -- a pair's room key (see Pair) does.
  std::string identity;

  bool operator==(const Participant&) const = default;
};

// The game and where in it everyone is: the child indices from the root.
struct GameState {
  std::string      sgf;
  std::vector<int> path;
};

// Points of a line someone is drawing, in board coordinates -- in points,
// the corner point's centre at (0, 0) -- so they land in the same place on
// every screen. Sent as they are drawn, a few at a time.
struct StrokePart {
  uint32_t                          stroke = 0;  // the author's own number for the line
  std::vector<std::pair<float, float>> points;
  bool                              finished = false;
};

class Writer {
public:
  explicit Writer(Type type) { this->out += char(type); }
  Writer& u8(uint8_t v) { this->out += char(v); return *this; }
  Writer& u32(uint32_t v)
  {
    for (int i = 0; i < 4; ++i) this->out += char(v >> (i * 8));
    return *this;
  }
  Writer& f32(float v)
  {
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    return this->u32(bits);
  }
  Writer& str(std::string_view s)
  {
    this->u32(uint32_t(s.size()));
    this->out += s;
    return *this;
  }
  Writer& path(const std::vector<int>& p)
  {
    this->u32(uint32_t(p.size()));
    for (int i : p) this->u32(uint32_t(i));
    return *this;
  }
  Writer& participant(const Participant& p) { return this->u32(p.id).str(p.name).u8(p.colour).str(p.identity); }
  Writer& state(const GameState& s) { return this->str(s.sgf).path(s.path); }
  const std::string& bytes() const { return this->out; }

private:
  std::string out;
};

// Reads what a Writer wrote. Reading past the end, or anything that doesn't
// add up, makes it !ok() and every later read a zero -- so a message is read
// through and then checked once.
class Reader {
public:
  explicit Reader(std::string_view in) : in(in) {}
  Type     type() { return Type(this->u8()); }
  uint8_t  u8() { return this->has(1) ? uint8_t(this->in[this->at++]) : 0; }
  uint32_t u32()
  {
    if (!this->has(4)) return 0;
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= uint32_t(uint8_t(this->in[this->at + size_t(i)])) << (i * 8);
    this->at += 4;
    return v;
  }
  float f32()
  {
    const uint32_t bits = this->u32();
    float v;
    std::memcpy(&v, &bits, 4);
    return v;
  }
  std::string str()
  {
    const uint32_t n = this->u32();
    if (!this->has(n)) return {};
    std::string s(this->in.substr(this->at, n));
    this->at += n;
    return s;
  }
  std::vector<int> path()
  {
    const uint32_t n = this->u32();
    std::vector<int> p;
    if (!this->has(size_t(n) * 4)) return p;
    for (uint32_t i = 0; i < n; ++i) p.push_back(int(this->u32()));
    return p;
  }
  Participant participant()
  {
    Participant p;
    p.id     = this->u32();
    p.name   = this->str();
    p.colour = this->u8();
    p.identity = this->str();
    return p;
  }
  GameState state()
  {
    GameState s;
    s.sgf  = this->str();
    s.path = this->path();
    return s;
  }
  bool ok() const { return this->good; }

private:
  bool has(size_t n)
  {
    if (this->good && this->in.size() - this->at >= n) return true;
    this->good = false;
    return false;
  }
  std::string_view in;
  size_t           at   = 0;
  bool             good = true;
};

}  // namespace net
