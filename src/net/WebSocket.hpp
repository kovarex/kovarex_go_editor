// WebSocket (RFC 6455), the little of it the editor needs: binary messages
// over a Socket, as a client or as a server. What the editors say to each
// other goes over this whether they are connected directly or through the
// relay -- a web host puts a program on the internet only behind its web
// proxy, and a WebSocket goes through one.
//
// Like Socket, nothing waits: poll() does what can be done and returns.

#pragma once

#include <net/Socket.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace net {

class WebSocket {
public:
  // The client side of a connection made to `host`: asks it to upgrade the
  // request for `path` to a WebSocket.
  static std::unique_ptr<WebSocket> client(std::unique_ptr<Socket> socket, const std::string& host, const std::string& path);
  // The server side of a connection someone made: waits for their request.
  static std::unique_ptr<WebSocket> server(std::unique_ptr<Socket> socket);

  // Queued until the handshake is done, then sent as one binary message.
  void send(std::string_view message);

  // Moves what can be moved, and adds the messages that arrived whole to
  // `messages`. False once the connection is over; `error()` then says why,
  // if something went wrong.
  bool poll(std::vector<std::string>& messages);

  // Whether the handshake is done.
  bool isOpen() const { return this->open; }
  const std::string& error() const { return this->problem; }
  const std::string& peer() const { return this->socket->peer(); }

  // Says goodbye properly, as far as the other side will listen.
  void close();

private:
  WebSocket(std::unique_ptr<Socket> socket, bool isClient);

  bool handshake();
  bool frames(std::vector<std::string>& messages);
  void frame(int opcode, std::string_view payload);
  bool fail(std::string why);

  std::unique_ptr<Socket> socket;
  bool                    isClient;
  bool                    open    = false;
  bool                    closing = false;
  std::string             key;       // the client's Sec-WebSocket-Key
  std::string             in;        // bytes read and not yet made sense of
  std::vector<std::string> pending;  // messages to send once open
  std::string             partial;   // a message arriving in several frames
  std::string             problem;

  // For keeping a quiet connection alive through proxies that drop idle ones,
  // and noticing when the other side is gone without a word.
  std::chrono::steady_clock::time_point lastSent  = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point lastHeard = std::chrono::steady_clock::now();
};

}  // namespace net
