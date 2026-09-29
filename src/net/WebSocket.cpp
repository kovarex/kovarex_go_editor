#include <net/WebSocket.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <random>

namespace net {

namespace {

// What the server hashes the client's key with to show it speaks WebSocket.
constexpr std::string_view MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Bigger than any game record; a guard against a broken or hostile peer.
constexpr uint64_t MAX_MESSAGE = 16u << 20;

enum Opcode { CONTINUATION = 0, TEXT = 1, BINARY = 2, CLOSE = 8, PING = 9, PONG = 10 };

// A connection with nothing to say pings this often, so the proxies between
// don't take it for dead -- many drop one idle for a minute. One that hears
// nothing at all, not even the answer to a ping, for QUIET_LIMIT is taken for
// gone.
constexpr auto PING_EVERY  = std::chrono::seconds(20);
constexpr auto QUIET_LIMIT = std::chrono::seconds(70);

// SHA-1, for the handshake only -- nothing here relies on it being secure.
std::array<uint8_t, 20> Sha1(std::string_view text)
{
  uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
  std::string data(text);
  const uint64_t bits = uint64_t(text.size()) * 8;
  data += static_cast<char>(0x80);  // the one bit after the message
  while (data.size() % 64 != 56) data += char(0);
  for (int i = 7; i >= 0; --i) data += char(bits >> (i * 8));
  const auto rotl = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
  for (size_t chunk = 0; chunk < data.size(); chunk += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
      w[i] = uint32_t(uint8_t(data[chunk + i * 4])) << 24 | uint32_t(uint8_t(data[chunk + i * 4 + 1])) << 16 |
             uint32_t(uint8_t(data[chunk + i * 4 + 2])) << 8 | uint32_t(uint8_t(data[chunk + i * 4 + 3]));
    }
    for (int i = 16; i < 80; ++i) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20)      f = (b & c) | (~b & d), k = 0x5A827999;
      else if (i < 40) f = b ^ c ^ d,           k = 0x6ED9EBA1;
      else if (i < 60) f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
      else             f = b ^ c ^ d,           k = 0xCA62C1D6;
      const uint32_t t = rotl(a, 5) + f + e + k + w[i];
      e = d, d = c, c = rotl(b, 30), b = a, a = t;
    }
    h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
  }
  std::array<uint8_t, 20> digest{};
  for (int i = 0; i < 20; ++i) digest[size_t(i)] = uint8_t(h[i / 4] >> (24 - (i % 4) * 8));
  return digest;
}

std::string Base64(const uint8_t* bytes, size_t n)
{
  static constexpr char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < n; i += 3) {
    const uint32_t v = uint32_t(bytes[i]) << 16 | (i + 1 < n ? uint32_t(bytes[i + 1]) << 8 : 0) | (i + 2 < n ? bytes[i + 2] : 0);
    out += ALPHABET[v >> 18 & 63];
    out += ALPHABET[v >> 12 & 63];
    out += i + 1 < n ? ALPHABET[v >> 6 & 63] : '=';
    out += i + 2 < n ? ALPHABET[v & 63] : '=';
  }
  return out;
}

std::string Accept(std::string_view key)
{
  const auto digest = Sha1(std::string(key) + std::string(MAGIC));
  return Base64(digest.data(), digest.size());
}

uint32_t Random()
{
  static std::mt19937 generator{ std::random_device{}() };
  return generator();
}

// A header's value from an HTTP request or response, its name matched
// ignoring case; empty if it isn't there.
std::string Header(std::string_view head, std::string_view name)
{
  std::string lower(head);
  std::transform(lower.begin(), lower.end(), lower.begin(), [](char c) { return char(std::tolower(static_cast<unsigned char>(c))); });
  std::string wanted = "\r\n" + std::string(name) + ":";
  std::transform(wanted.begin(), wanted.end(), wanted.begin(), [](char c) { return char(std::tolower(static_cast<unsigned char>(c))); });
  const size_t at = lower.find(wanted);
  if (at == std::string::npos) return {};
  size_t       from = at + wanted.size();
  const size_t to   = head.find("\r\n", from);
  while (from < to && head[from] == ' ') ++from;
  return std::string(head.substr(from, to - from));
}

}  // namespace

WebSocket::WebSocket(std::unique_ptr<Socket> socket, bool isClient)
    : socket(std::move(socket))
    , isClient(isClient)
{}

std::unique_ptr<WebSocket> WebSocket::client(std::unique_ptr<Socket> socket, const std::string& host, const std::string& path)
{
  std::unique_ptr<WebSocket> ws(new WebSocket(std::move(socket), true));
  uint8_t nonce[16];
  for (uint8_t& b : nonce) b = uint8_t(Random());
  ws->key = Base64(nonce, sizeof(nonce));
  ws->socket->send("GET " + path + " HTTP/1.1\r\n"
                   "Host: " + host + "\r\n"
                   "Upgrade: websocket\r\n"
                   "Connection: Upgrade\r\n"
                   "Sec-WebSocket-Key: " + ws->key + "\r\n"
                   "Sec-WebSocket-Version: 13\r\n\r\n");
  return ws;
}

std::unique_ptr<WebSocket> WebSocket::server(std::unique_ptr<Socket> socket)
{
  return std::unique_ptr<WebSocket>(new WebSocket(std::move(socket), false));
}

void WebSocket::send(std::string_view message)
{
  if (this->open) this->frame(BINARY, message);
  else            this->pending.emplace_back(message);
}

void WebSocket::close()
{
  if (this->open && !this->closing) this->frame(CLOSE, {});
  this->closing = true;
  std::string ignored;
  this->socket->pump(ignored);
}

bool WebSocket::fail(std::string why)
{
  if (this->problem.empty()) this->problem = std::move(why);
  return false;
}

bool WebSocket::poll(std::vector<std::string>& messages)
{
  const auto   now    = std::chrono::steady_clock::now();
  const size_t before = this->in.size();
  const bool   alive  = this->socket->pump(this->in);
  if (this->in.size() != before) this->lastHeard = now;
  if (now - this->lastHeard > QUIET_LIMIT) return this->fail("The connection went quiet.");
  if (this->open && !this->closing && now - this->lastSent > PING_EVERY) this->frame(PING, {});
  bool ok = true;
  if (!this->open) ok = this->handshake();
  if (ok && this->open) ok = this->frames(messages);
  // What the handshake or the frames queued up -- replies, pongs -- sent now
  // rather than a frame later. What arrives meanwhile waits for the next poll.
  std::string more;
  const bool stillAlive = this->socket->pump(more);
  if (!more.empty()) this->lastHeard = now;
  this->in += more;
  if (!ok) return false;
  if (!alive || !stillAlive) return this->fail("The connection was closed.");
  return !this->closing;
}

bool WebSocket::handshake()
{
  const size_t end = this->in.find("\r\n\r\n");
  if (end == std::string::npos) {
    if (this->in.size() > 16384) return this->fail("That isn't a WebSocket server.");
    return true;  // not all of it yet
  }
  const std::string head = this->in.substr(0, end + 2);
  this->in.erase(0, end + 4);

  if (this->isClient) {
    if (head.compare(0, 12, "HTTP/1.1 101") != 0 || Header(head, "Sec-WebSocket-Accept") != Accept(this->key)) {
      return this->fail("The server didn't accept the connection (" + head.substr(0, head.find("\r\n")) + ").");
    }
  } else {
    const std::string clientKey = Header(head, "Sec-WebSocket-Key");
    if (clientKey.empty()) {
      // An ordinary web request -- a browser pointed at the address, say:
      // answered, so it can be seen that something is there.
      this->socket->send("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"
                         "Go Editor. Connect to this address from the editor, not a browser.\n");
      this->closing = true;
      return this->fail("A web browser, not an editor.");
    }
    this->socket->send("HTTP/1.1 101 Switching Protocols\r\n"
                       "Upgrade: websocket\r\n"
                       "Connection: Upgrade\r\n"
                       "Sec-WebSocket-Accept: " + Accept(clientKey) + "\r\n\r\n");
  }
  this->open = true;
  for (const std::string& message : this->pending) this->frame(BINARY, message);
  this->pending.clear();
  return true;
}

bool WebSocket::frames(std::vector<std::string>& messages)
{
  for (;;) {
    const auto* b = reinterpret_cast<const uint8_t*>(this->in.data());
    const size_t size = this->in.size();
    if (size < 2) return true;
    const bool fin    = b[0] & 0x80;
    const int  opcode = b[0] & 0x0F;
    const bool masked = b[1] & 0x80;
    uint64_t   length = b[1] & 0x7F;
    size_t     at     = 2;
    if (length == 126) {
      if (size < 4) return true;
      length = uint64_t(b[2]) << 8 | b[3];
      at     = 4;
    } else if (length == 127) {
      if (size < 10) return true;
      length = 0;
      for (int i = 0; i < 8; ++i) length = length << 8 | b[2 + i];
      at = 10;
    }
    if (length > MAX_MESSAGE) return this->fail("A message too big to be a game.");
    uint8_t mask[4] = {};
    if (masked) {
      if (size < at + 4) return true;
      std::copy(b + at, b + at + 4, mask);
      at += 4;
    }
    if (size < at + length) return true;  // not all of it yet

    std::string payload = this->in.substr(at, size_t(length));
    if (masked) {
      for (size_t i = 0; i < payload.size(); ++i) payload[i] = char(payload[i] ^ mask[i % 4]);
    }
    this->in.erase(0, at + size_t(length));

    switch (opcode) {
    case TEXT:
    case BINARY:
    case CONTINUATION:
      this->partial += payload;
      if (this->partial.size() > MAX_MESSAGE) return this->fail("A message too big to be a game.");
      if (fin) {
        messages.push_back(std::move(this->partial));
        this->partial.clear();
      }
      break;
    case PING:
      this->frame(PONG, payload);
      break;
    case CLOSE:
      if (!this->closing) this->frame(CLOSE, {});
      this->closing = true;
      return false;
    default:
      break;  // pongs, and whatever else: nothing to do
    }
  }
}

void WebSocket::frame(int opcode, std::string_view payload)
{
  this->lastSent = std::chrono::steady_clock::now();
  std::string head;
  head += char(0x80 | opcode);
  const uint8_t maskBit = this->isClient ? 0x80 : 0;  // clients mask what they send; servers don't
  if (payload.size() < 126) {
    head += char(maskBit | payload.size());
  } else if (payload.size() < 65536) {
    head += char(maskBit | 126);
    head += char(payload.size() >> 8);
    head += char(payload.size());
  } else {
    head += char(maskBit | 127);
    for (int i = 7; i >= 0; --i) head += char(uint64_t(payload.size()) >> (i * 8));
  }
  if (!this->isClient) {
    this->socket->send(head);
    this->socket->send(payload);
    return;
  }
  const uint32_t word    = Random();
  const uint8_t  mask[4] = { uint8_t(word), uint8_t(word >> 8), uint8_t(word >> 16), uint8_t(word >> 24) };
  head.append(reinterpret_cast<const char*>(mask), 4);
  std::string body(payload);
  for (size_t i = 0; i < body.size(); ++i) body[i] = char(body[i] ^ mask[i % 4]);
  this->socket->send(head);
  this->socket->send(body);
}

}  // namespace net
