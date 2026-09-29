# The relay

A small server for sharing a game when nobody can host one: the teacher and
the students all connect to the relay, and it passes the game between them.
It is `relay/Relay.cpp` and the editor's own `src/net/` code, and needs
nothing but a C++20 compiler.

In the editor (Online page), the relay's address goes in *Address*, as
`ws://host[:port][/path]`:

- joining with no *Room* opens a new room with the game in the editor, and
  shows its code, such as `WJA8YC`;
- the others join with the same address and that code.

Two people who met in a room are remembered by each other's editor, with a
long room code of their own (20 letters, too many to guess). Next time, each
picks the other under *People you met* on the Online page, and they meet in
that room: the relay makes it for whichever of them comes first. It keeps
no list of people or codes: those are only in the two editors' settings.

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

NearlyFreeSpeech runs long-lived programs as *daemons*, and only lets them be
reached through the site's web server, as a *proxy*. WebSockets pass through
proxies, so the relay works there on plain `ws://` over port 80.

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
   - Tag: `go-relay`
   - Command line: `/home/protected/go_relay 27272`
   - Working directory: `/home/protected`
   - Run daemon as: `me`

   Daemons need a site whose server type allows them. If *Daemons* isn't
   offered, change the server type in the site's config to one that is.

3. **Proxy.** In the site's *Proxies* section, add one:
   - Protocol: `HTTP`
   - Base URI: `/relay`
   - Document root: `/`
   - Target port: `27272`

4. **Check it.** Opening `http://<site>.nfshost.com/relay` in a browser
   answers *"Go Editor. Connect to this address from the editor, not a
   browser."*. In the editor, the address is
   `ws://<site>.nfshost.com/relay`.

Updating it: `git pull` and build again in `/home/protected/kovarex_go_editor`,
copy the new `go_relay` over the old one, and restart the daemon.

The relay spends almost nothing while idle: it looks for work 25 times a
second, and faster only while messages are going through.
