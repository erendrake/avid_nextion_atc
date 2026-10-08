# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Arduino firmware for a Nextion touchscreen pendant that will drive an AVID CNC
automatic tool changer. Currently only a bring-up sketch (`Nextion_Tester`)
exists; it proves two-way serial communication with the display. There is no
test suite and no linter. "Verification" means it compiles for every profile
and, ideally, behaves on real hardware (which CI cannot check).

## Build commands

Everything goes through arduino-cli using the profiles in
`Nextion_Tester/sketch.yaml` (`uno`, `nano`, `mega`). Profiles pin the
`arduino:avr@1.8.6` core and point at the vendored libraries, so no global
library or core installs are needed.

```powershell
.\build.ps1                          # compile, default profile uno
.\build.ps1 -Profile mega            # compile for another profile
.\build.ps1 build -RawDump           # compile with RAW_DUMP=1 (raw byte dump, no event decoding)
.\build.ps1 upload -Port COM7        # compile + upload
.\build.ps1 monitor -Port COM7       # 115200 baud serial monitor
.\build.ps1 ports                    # list serial ports
```

Raw arduino-cli equivalents (any OS):

```sh
arduino-cli compile --profile uno Nextion_Tester
arduino-cli compile --profile uno --build-property build.extra_flags=-DRAW_DUMP=1 Nextion_Tester
```

Before claiming a change builds, compile **all three profiles**. The mega
profile exercises a different code path (hardware `Serial1` instead of
SoftwareSerial), so an Uno-only compile is not sufficient. CI
(`.github/workflows/compile.yml`) does the same three builds.

arduino-cli's default is `--warnings none`. Turning warnings on floods the
output with `-Wwrite-strings` from the vendored NeoNextion; that noise is
expected and not something to fix.

## Architecture

**Serial ownership is the one rule that matters.** The Nextion display sends
touch events as 7-byte frames (`65 page comp 01|00 FF FF FF`). NeoNextion's
`Nextion::poll()` parses these only when it reads the `0x65` header itself.
Any other `read()` on the display's serial port, or any blocking library call
such as `getText()` in `loop()`, steals or discards frames and events silently
stop arriving. This was the project's original bug. `loop()` must call
`nex.poll()` and nothing that blocks or reads the display port. The only
sanctioned way to look at raw bytes is the `RAW_DUMP` build, which disables
event decoding entirely.

**Port selection is compile-time.** The sketch checks `HAVE_HWSERIAL1` (defined
by the AVR core on Mega/Leonardo/Micro) and aliases `nextionSerial` to
`Serial1`; otherwise it instantiates SoftwareSerial on pins 10/11. Keep any new
code using the `nextionSerial` name rather than a concrete port.

**Widgets are declared globally and self-register.** Each `NextionButton`
(or other `INextionTouchable`) constructor appends itself to a linked list
inside the `Nextion` object. `poll()` walks that list and dispatches to
whichever widget's page and component id match the frame. Consequently:

- The `page` and `component` arguments must match the `id` attribute shown in
  the Nextion Editor, not the `objname`. The name string is used only for
  outgoing commands like `setText`.
- Touch events only arrive if "Send Component ID" is ticked on both the
  Touch Press and Touch Release tabs of that component in the HMI. This is
  configured on the display side and cannot be fixed in firmware.
- Callbacks are plain function pointers with the signature
  `void f(NextionEventType, INextionTouchable *)`, attached via
  `attachCallback`. `NEX_EVENT_PUSH` is press, `NEX_EVENT_POP` is release.

**Library API quirks.** NeoNextion takes `char *`, not `const char *`, so
string literals passed to `setText` and similar need a `(char *)` cast. Its
`receiveNumber` is broken for values above 65535 on AVR (24-bit shift through a
16-bit int). `nex.init()` sets `bkcmd=1` so command-completion checks work.

## Vendored libraries

`libraries/NeoNextion` (GPL v2) and `libraries/AccelStepper` (GPL v3, not yet
used by any sketch) are committed copies, not library-manager installs. Do not
edit them in place. If upstream changes are needed, bump the whole directory
from upstream and note it in the README's licensing section. The project's own
code is MIT.

## Conventions

- The `.HMI` / `.tft` display project is not in the repo yet; when added it
  belongs under `hmi/` so firmware and screen layout version together.
- Component ids and page numbers live in `#define`s at the top of the sketch;
  update them there when the HMI changes rather than inlining numbers.
- When changing how the display is talked to, update the troubleshooting
  ladder in `README.md`, which is written as the first stop for "no events".
