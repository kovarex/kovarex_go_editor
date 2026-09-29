#!/bin/sh
# Builds the relay, with nothing but a C++20 compiler -- on FreeBSD (where
# c++ is clang) or Linux:
#   sh relay/build.sh
# and the program is relay/go_relay. CXX picks another compiler.
set -e
cd "$(dirname "$0")/.."
${CXX:-c++} -std=c++20 -O2 -Wall -I src -o relay/go_relay \
  relay/Relay.cpp src/net/Hub.cpp src/net/Socket.cpp src/net/WebSocket.cpp
echo "Built relay/go_relay"
