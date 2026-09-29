# The relay

A small server for sharing a game when nobody can host one: the teacher and
the students all connect to the relay, and it passes the game between them.
It is `relay/Relay.cpp` and the editor's own `src/net/` code, and needs
nothing but a C++20 compiler.

On the editor's Online page:

- *Start* opens a new session -- a room on the relay -- with the game in the
  editor, and shows its invite code, such as `WJA8YC`, to give to someone
  new; they come in with *Invite code* and *Join*.
- Two people who met in a session are remembered by each other's editor,
  with a long key of their own (20 letters, too many to guess), and listed
  under *People you met*. *Connect* there goes, without any code, to the
  session the other one is in -- each editor tells the relay which keys lead
  to it, wherever it is. If they are in none, it waits in a room of the two
  of them, and the relay sends it on as soon as the other one turns up in a
  session.
- The relay's address, `ws://kovarexgoeditor.com` unless changed, is under
  *Other ways to connect*, with hosting and joining without a relay.

The relay keeps no list of people or keys: those are only in the editors'
settings, and it knows only who is where right now.

A room nobody is in is kept for 30 minutes, game and all, so whoever lost
their connection can come back to it. An address that tries ten wrong codes
is turned away for ten minutes.

## Building and running

```sh
sh relay/build.sh          # makes relay/go_relay
relay/go_relay 27272       # the port; 27272 unless told otherwise
```

It logs rooms opening and closing and people coming in to stdout.

## On NearlyFreeSpeech

The project's relay runs there, as the site `kovarexgoeditor` (server type
*Custom*) at `ws://kovarexgoeditor.com`, which the editor offers by default.
NearlyFreeSpeech runs long-lived programs as *daemons*, reached only through
the site's web server, as a *proxy*.

1. **Build it there.** Over ssh:

   ```sh
   cd /home/protected
   git clone https://github.com/kovarex/kovarex_go_editor.git
   sh kovarex_go_editor/relay/build.sh
   cp kovarex_go_editor/relay/go_relay /home/protected/
   ```

   (NearlyFreeSpeech runs FreeBSD, whose `c++` is clang; a binary built on
   Linux won't run there.)

2. **Daemon.** In the site's *Daemons* section, add one:
   - Tag: `gorelay` (letters only)
   - Command line: `/home/protected/go_relay` -- no arguments allowed there,
     so it listens on its default port, 27272
   - Working directory: `/home/protected`
   - Run daemon as: `me`

3. **Proxy.** In the site's *Proxies* section, add one:
   - Protocol: `WebSockets` -- an `HTTP` proxy turns WebSocket connections
     away (*400 Bad Request*)
   - Base URI: `/`
   - Document root: `/`
   - Target port: `27272`

4. **Check it.** A browser gets *"WebSockets Only"* from NearlyFreeSpeech,
   which means the proxy is there; in the editor, *Start* opens a session.

Updating it: `git pull` and build again in `/home/protected/kovarex_go_editor`,
copy the new `go_relay` over the old one, and restart the daemon.

The relay spends almost nothing while idle: it looks for work 25 times a
second, and faster only while messages are going through.
