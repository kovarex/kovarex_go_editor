#!/bin/sh
# Updates the relay where it runs (see README.md, "On NearlyFreeSpeech"):
# pulls the repository, builds the relay, puts it in place of the running
# one, and has the daemon start the new one. From anywhere:
#   sh /home/protected/kovarex_go_editor/relay/deploy.sh
#
# All of it is in main(), read whole before anything runs, since the pull
# may change this very file.

main() {
  set -e
  repository=$(cd "$(dirname "$0")/.." && pwd)
  target=/home/protected/go_relay

  cd "$repository"
  git pull --ff-only
  sh relay/build.sh

  # A running program's file can't be written over, but it can be replaced:
  # the new one goes next to it and is renamed over it.
  cp relay/go_relay "$target.new"
  mv -f "$target.new" "$target"

  # The daemon starts it again when it stops: stopped, it comes back as the
  # new one.
  if pkill -x go_relay; then
    echo "Stopped the old relay; waiting for the daemon to start the new one..."
    sleep 3
  fi
  if pgrep -x go_relay > /dev/null; then
    echo "Done: the new relay is running."
  else
    echo "The relay isn't running: restart the daemon on the site's page."
    exit 1
  fi
}

main "$@"
exit
