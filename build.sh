#!/usr/bin/env bash
#
# Build, upload and monitor the sketches in this repo with arduino-cli.
# Works in Git Bash on Windows, macOS and Linux. Uses the profiles in each
# sketch's sketch.yaml, so nothing is installed globally except arduino-cli.
#
# Usage:
#   ./build.sh                      compile Nextion_Tester for the default profile (nano_every)
#   ./build.sh build -p mega        compile for another profile
#   ./build.sh upload               compile + upload, auto-detect the port
#   ./build.sh upload -P COM7       compile + upload to a specific port
#   ./build.sh monitor -P COM7      open the 115200 baud debug monitor
#   ./build.sh all -P COM7          compile, upload, then monitor
#   ./build.sh upload --raw-dump    RAW_DUMP=1 build (print bytes, no event decoding)
#   ./build.sh upload -s NextionBridge   flash the USB<->display bridge (for TFT uploads)
#   ./build.sh ports                list serial ports and detected boards
#   ./build.sh install              install arduino-cli (winget / brew / curl)
#
# Options:
#   -s, --sketch   Nextion_Tester|NextionBridge  (default: Nextion_Tester)
#   -p, --profile  nano_every|uno|nano|mega      (default: nano_every)
#   -P, --port     COMx or /dev/tty* (default: auto-detect)
#   -b, --baud     monitor baud     (default: 115200)
#   --raw-dump                       compile with -DRAW_DUMP=1
#   --display-baud N                 compile with -DDISPLAY_BAUD=N (NextionBridge: the display's current baud)
#   -v, --verbose                    show compiler warnings

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

task="build"
sketch_name="Nextion_Tester"
profile="nano_every"
port=""
baud="115200"
raw_dump=0
display_baud=""
warnings="none"

usage() { sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; }

while [ $# -gt 0 ]; do
  case "$1" in
    build|upload|monitor|all|ports|install) task="$1" ;;
    -s|--sketch)  sketch_name="$2"; shift ;;
    -p|--profile) profile="$2"; shift ;;
    -P|--port)    port="$2"; shift ;;
    -b|--baud)    baud="$2"; shift ;;
    --raw-dump)   raw_dump=1 ;;
    --display-baud) display_baud="$2"; shift ;;
    -v|--verbose) warnings="default" ;;
    -h|--help)    usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

case "$profile" in nano_every|uno|nano|mega) ;; *) echo "unknown profile: $profile (nano_every|uno|nano|mega)" >&2; exit 2 ;; esac

sketch="$here/$sketch_name"
if [ ! -f "$sketch/sketch.yaml" ]; then
  echo "unknown sketch: $sketch_name (no $sketch_name/sketch.yaml in repo)" >&2
  exit 2
fi

say() { printf '\033[36m%s\033[0m\n' "$*"; }
warn() { printf '\033[33m%s\033[0m\n' "$*"; }

assert_cli() {
  command -v arduino-cli >/dev/null 2>&1 && return
  echo "arduino-cli not found on PATH. Run './build.sh install' or see README.md." >&2
  echo "On Windows it installs to 'C:\\Program Files\\Arduino CLI'; open a new shell after installing." >&2
  exit 1
}

find_port() {
  if [ -n "$port" ]; then echo "$port"; return; fi
  # Columns: Port Protocol Type Board FQBN Core. Prefer a port with a recognised
  # board; skip macOS's Bluetooth pseudo-port.
  local list
  list="$(arduino-cli board list 2>/dev/null | awk 'NR>1 && $2=="serial"' | grep -vi bluetooth || true)"
  local pick
  pick="$(printf '%s\n' "$list" | grep -m1 'arduino:' | awk '{print $1}' || true)"
  [ -z "$pick" ] && pick="$(printf '%s\n' "$list" | head -n1 | awk '{print $1}')"
  if [ -z "$pick" ]; then
    echo "No serial port found. Plug the board in or pass -P <port>." >&2
    exit 1
  fi
  say "Using port $pick" >&2
  echo "$pick"
}

do_build() {
  local extra=() flags=""
  if [ "$raw_dump" = 1 ]; then
    warn "RAW_DUMP=1: events will not be decoded"
    flags="$flags -DRAW_DUMP=1"
  fi
  if [ -n "$display_baud" ]; then
    warn "DISPLAY_BAUD=$display_baud"
    flags="$flags -DDISPLAY_BAUD=$display_baud"
  fi
  [ -n "$flags" ] && extra+=(--build-property "build.extra_flags=$flags")
  say "Compiling $sketch for profile '$profile'..."
  # Warnings default to off: the vendored NeoNextion library emits dozens of
  # -Wwrite-strings warnings that drown out anything from the sketch.
  arduino-cli compile --profile "$profile" --warnings "$warnings" ${extra[@]+"${extra[@]}"} "$sketch"
}

do_upload() {
  local p; p="$(find_port)"
  say "Uploading to $p..."
  arduino-cli upload --profile "$profile" -p "$p" "$sketch"
}

do_monitor() {
  local p; p="$(find_port)"
  say "Monitor on $p at $baud baud (Ctrl+C to exit)"
  arduino-cli monitor -p "$p" --config "baudrate=$baud"
}

do_install() {
  case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
      winget install --id ArduinoSA.CLI -e --accept-source-agreements --accept-package-agreements
      echo "Open a new shell so arduino-cli is on PATH, then run ./build.sh" ;;
    Darwin)
      brew install arduino-cli ;;
    *)
      mkdir -p "$HOME/.local/bin"
      curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR="$HOME/.local/bin" sh
      echo "Make sure $HOME/.local/bin is on PATH, then run ./build.sh" ;;
  esac
}

case "$task" in
  install) do_install ;;
  ports)   assert_cli; arduino-cli board list ;;
  build)   assert_cli; do_build ;;
  upload)  assert_cli; do_build; do_upload ;;
  monitor) assert_cli; do_monitor ;;
  all)     assert_cli; do_build; do_upload; do_monitor ;;
esac
