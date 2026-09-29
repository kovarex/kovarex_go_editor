// TCP, as plainly as it can be put and the same on Windows and Linux: a
// connection that is never waited on -- pump() moves whatever can be moved
// this frame and returns -- and a listener that accepts them.
//
// Nothing here includes raylib or windows.h in its header (Windows' socket
// headers are windows.h's, and it and raylib.h can't share a file).

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace net {

class Socket {
public:
  // Connects to `host` (a name or an address) on `port`. Blocks for as long
  // as looking the name up and connecting take, so it is for a thread of its
  // own. Null with the reason in `error` if it can't.
  static std::unique_ptr<Socket> connect(const std::string& host, int port, std::string* error);

  ~Socket();
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  // Queued, and sent by pump() as fast as the other side takes it.
  void send(std::string_view bytes);

  // Reads whatever has arrived onto the end of `in`, and writes what it can
  // of what is queued. False once the connection is closed, from either side.
  bool pump(std::string& in);

  // Who is at the other end, as "address:port".
  const std::string& peer() const { return this->peerName; }

private:
  friend class Listener;
  explicit Socket(std::intptr_t handle, std::string peer);

  std::intptr_t handle;
  std::string   out;
  std::string   peerName;
  bool          closed = false;
};

class Listener {
public:
  // Listens on `port` on every address the machine has. Null with the reason
  // in `error` if the port can't be had.
  static std::unique_ptr<Listener> open(int port, std::string* error);

  ~Listener();
  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;

  // A connection someone made, or null if nobody is waiting.
  std::unique_ptr<Socket> accept();

private:
  explicit Listener(std::intptr_t handle);
  std::intptr_t handle;
};

// This machine's addresses -- the ones it reaches the internet from, IPv4 and
// IPv6 -- for telling others where to connect to it.
std::vector<std::string> LocalAddresses();

}  // namespace net
