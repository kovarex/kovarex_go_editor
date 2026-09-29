#!/bin/sh
# Updates the relay where it runs (see README.md, "On NearlyFreeSpeech"):
# pulls the repository, builds the relay, and puts it in place of the
# running one. From anywhere:
#   sh /home/protected/kovarex_go_editor/relay/deploy.sh
#
# The running relay notices within seconds that its program was replaced and
# stops, and the daemon starts the new one. (Daemons don't run where ssh
# does, so they can't be stopped from here.)
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
  echo "Done: the relay starts the new build within seconds."
}

main "$@"
exit
