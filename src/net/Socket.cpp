#include <net/Socket.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

#include <cstring>
#include <vector>

namespace net {

namespace {

#ifdef _WIN32
using Handle                   = SOCKET;
constexpr Handle INVALID       = INVALID_SOCKET;
int  LastError() { return WSAGetLastError(); }
bool WouldBlock(int error) { return error == WSAEWOULDBLOCK; }
void Close(Handle h) { closesocket(h); }

// Winsock has to be started before anything else of it is used; once is
// enough, and it is never stopped -- the process ending does that.
void Start()
{
  static const bool started = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  (void)started;
}
#else
using Handle             = int;
constexpr Handle INVALID = -1;
int  LastError() { return errno; }
bool WouldBlock(int error) { return error == EAGAIN || error == EWOULDBLOCK; }
void Close(Handle h) { ::close(h); }
void Start() {}
#endif

void NonBlocking(Handle h)
{
#ifdef _WIN32
  u_long on = 1;
  ioctlsocket(h, FIONBIO, &on);
#else
  fcntl(h, F_SETFL, fcntl(h, F_GETFL, 0) | O_NONBLOCK);
#endif
}

// Moves are a few bytes: sent at once rather than held back to fill a packet.
void NoDelay(Handle h)
{
  int on = 1;
  setsockopt(h, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
}

std::string Describe(const sockaddr* address)
{
  char text[INET6_ADDRSTRLEN] = {};
  int  port = 0;
  if (address->sa_family == AF_INET) {
    const auto* in = reinterpret_cast<const sockaddr_in*>(address);
    inet_ntop(AF_INET, &in->sin_addr, text, sizeof(text));
    port = ntohs(in->sin_port);
  } else if (address->sa_family == AF_INET6) {
    const auto* in = reinterpret_cast<const sockaddr_in6*>(address);
    inet_ntop(AF_INET6, &in->sin6_addr, text, sizeof(text));
    port = ntohs(in->sin6_port);
  }
  return std::string(text) + ":" + std::to_string(port);
}

}  // namespace

Socket::Socket(std::intptr_t handle, std::string peer)
    : handle(handle)
    , peerName(std::move(peer))
{
  NonBlocking(Handle(handle));
  NoDelay(Handle(handle));
}

Socket::~Socket()
{
  Close(Handle(this->handle));
}

std::unique_ptr<Socket> Socket::connect(const std::string& host, int port, std::string* error)
{
  Start();
  addrinfo hints{};
  hints.ai_family   = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* found = nullptr;
  if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found) != 0 || !found) {
    if (error) *error = "Couldn't find " + host + ".";
    return nullptr;
  }
  // The first of its addresses that answers.
  std::unique_ptr<Socket> socket;
  for (addrinfo* a = found; a && !socket; a = a->ai_next) {
    const Handle h = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
    if (h == INVALID) continue;
    if (::connect(h, a->ai_addr, int(a->ai_addrlen)) == 0) {
      socket.reset(new Socket(std::intptr_t(h), Describe(a->ai_addr)));
    } else {
      Close(h);
    }
  }
  freeaddrinfo(found);
  if (!socket && error) *error = "Couldn't connect to " + host + ":" + std::to_string(port) + ".";
  return socket;
}

void Socket::send(std::string_view bytes)
{
  this->out.append(bytes);
}

bool Socket::pump(std::string& in)
{
  if (this->closed) return false;
  const Handle h = Handle(this->handle);

  char buffer[16384];
  for (;;) {
    const int n = int(::recv(h, buffer, int(sizeof(buffer)), 0));
    if (n > 0) {
      in.append(buffer, size_t(n));
      continue;
    }
    if (n == 0 || !WouldBlock(LastError())) this->closed = true;  // closed, or broken
    break;
  }

  while (!this->out.empty() && !this->closed) {
#ifdef MSG_NOSIGNAL
    constexpr int FLAGS = MSG_NOSIGNAL;  // a closed peer is an error, not a signal that ends the program
#else
    constexpr int FLAGS = 0;
#endif
    const int n = int(::send(h, this->out.data(), int(this->out.size()), FLAGS));
    if (n > 0) {
      this->out.erase(0, size_t(n));
    } else {
      if (!WouldBlock(LastError())) this->closed = true;
      break;
    }
  }
  return !this->closed;
}

Listener::Listener(std::intptr_t handle)
    : handle(handle)
{
  NonBlocking(Handle(handle));
}

Listener::~Listener()
{
  Close(Handle(this->handle));
}

std::unique_ptr<Listener> Listener::open(int port, std::string* error)
{
  Start();
  // IPv6 with IPv4 mapped onto it, so both kinds of address can connect; just
  // IPv4 where there is no IPv6 (in a jail without it, say).
  for (const bool six : { true, false }) {
    const Handle h = ::socket(six ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (h == INVALID) continue;
    if (six) {
      int off = 0;
      setsockopt(h, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off), sizeof(off));
    }
#ifndef _WIN32
    // A port just let go of can be had again at once, rather than in minutes.
    int on = 1;
    setsockopt(h, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif

    sockaddr_storage address{};
    int              length = 0;
    if (six) {
      auto* in = reinterpret_cast<sockaddr_in6*>(&address);
      in->sin6_family = AF_INET6;
      in->sin6_addr   = in6addr_any;
      in->sin6_port   = htons(uint16_t(port));
      length          = sizeof(sockaddr_in6);
    } else {
      auto* in = reinterpret_cast<sockaddr_in*>(&address);
      in->sin_family      = AF_INET;
      in->sin_addr.s_addr = htonl(INADDR_ANY);
      in->sin_port        = htons(uint16_t(port));
      length              = sizeof(sockaddr_in);
    }
    if (::bind(h, reinterpret_cast<sockaddr*>(&address), length) == 0 && ::listen(h, 16) == 0) {
      return std::unique_ptr<Listener>(new Listener(std::intptr_t(h)));
    }
    Close(h);
  }
  if (error) *error = "Couldn't listen on port " + std::to_string(port) + ": is something else using it?";
  return nullptr;
}

std::unique_ptr<Socket> Listener::accept()
{
  sockaddr_storage address{};
  socklen_t        length = sizeof(address);
  const Handle     h      = ::accept(Handle(this->handle), reinterpret_cast<sockaddr*>(&address), &length);
  if (h == INVALID) return nullptr;
  return std::unique_ptr<Socket>(new Socket(std::intptr_t(h), Describe(reinterpret_cast<sockaddr*>(&address))));
}


std::vector<std::string> LocalAddresses()
{
  Start();
  // The address this machine would reach the internet from: a UDP socket
  // "connected" towards a public address, which sends nothing, but makes the
  // system choose. Then IPv6 the same way.
  std::vector<std::string> found;
  struct Try {
    int         family;
    const char* address;
  };
  for (const Try t : { Try{ AF_INET, "8.8.8.8" }, Try{ AF_INET6, "2001:4860:4860::8888" } }) {
    const Handle h = ::socket(t.family, SOCK_DGRAM, IPPROTO_UDP);
    if (h == INVALID) continue;
    sockaddr_storage to{};
    int              length = 0;
    if (t.family == AF_INET) {
      auto* in = reinterpret_cast<sockaddr_in*>(&to);
      in->sin_family = AF_INET;
      in->sin_port   = htons(53);
      inet_pton(AF_INET, t.address, &in->sin_addr);
      length = sizeof(sockaddr_in);
    } else {
      auto* in = reinterpret_cast<sockaddr_in6*>(&to);
      in->sin6_family = AF_INET6;
      in->sin6_port   = htons(53);
      inet_pton(AF_INET6, t.address, &in->sin6_addr);
      length = sizeof(sockaddr_in6);
    }
    if (::connect(h, reinterpret_cast<sockaddr*>(&to), length) == 0) {
      sockaddr_storage mine{};
      socklen_t        mineLength = sizeof(mine);
      if (getsockname(h, reinterpret_cast<sockaddr*>(&mine), &mineLength) == 0) {
        std::string described = Describe(reinterpret_cast<sockaddr*>(&mine));
        found.push_back(described.substr(0, described.rfind(':')));  // the address, not the port
      }
    }
    Close(h);
  }
  return found;
}
}  // namespace net
