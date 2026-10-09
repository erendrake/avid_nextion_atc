#!/usr/bin/env bash
#
# Find the baud rate a Nextion display is set to. Reflashes NextionBridge at
# each candidate baud and sends the "connect" handshake through it. The baud
# that answers "comok" is the one the display is using.
#
# Usage:
#   tools/baud_scan.sh                      auto-detect port, try the usual rates
#   tools/baud_scan.sh -P /dev/cu.usbmodem14101
#   tools/baud_scan.sh -P COM3 -p mega 9600 115200
#
# Needs: arduino-cli on PATH, python3 with pyserial (see tools/nextion_probe.py).
# Leaves NextionBridge on the board at the last baud tried; reflash the
# tester afterwards with ./build.sh upload.

set -u

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(dirname "$here")"

port=""
profile="nano_every"
bauds=()

while [ $# -gt 0 ]; do
  case "$1" in
    -P|--port)    port="$2"; shift ;;
    -p|--profile) profile="$2"; shift ;;
    -h|--help)    sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) bauds+=("$1") ;;
  esac
  shift
done

[ ${#bauds[@]} -eq 0 ] && bauds=(9600 115200 57600 38400 19200 4800 2400)

# macOS ships bash 3.2, which treats an empty array as unset under `set -u`,
# hence the ${arr[@]+"${arr[@]}"} form.
flashport=()
probeport=()
if [ -n "$port" ]; then
  flashport=(-P "$port")
  probeport=(-p "$port")
fi

for b in "${bauds[@]}"; do
  echo "===== $b ====="
  if ! "$repo/build.sh" upload -s NextionBridge -p "$profile" ${flashport[@]+"${flashport[@]}"} --display-baud "$b" > /dev/null 2>&1; then
    echo "flash failed (run ./build.sh upload -s NextionBridge --display-baud $b to see why)"
    continue
  fi
  python3 "$here/nextion_probe.py" connect -b "$b" ${probeport[@]+"${probeport[@]}"} | grep -E "connect|sendme|bauds"
done
