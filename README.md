# avid_nextion_atc

Firmware for a touchscreen pendant that drives an AVID CNC automatic tool
changer (ATC). A [Nextion](https://nextion.tech/) HMI display talks to an
Arduino over serial; the Arduino reads touch events from the display, drives
the ATC hardware (drawbar, air blast, coolant, dust brush) and writes status
back to the screen.

The project is in the bring-up phase. `Nextion_Tester` is the only sketch so
far and its job is to prove the display and the Arduino can talk both ways.

[![Compile sketches](https://github.com/erendrake/avid_nextion_atc/actions/workflows/compile.yml/badge.svg)](https://github.com/erendrake/avid_nextion_atc/actions/workflows/compile.yml)

## Repository layout

| Path | What it is |
| --- | --- |
| `Nextion_Tester/Nextion_Tester.ino` | Bring-up sketch: two buttons, callbacks, status LED, raw byte dump mode |
| `Nextion_Tester/sketch.yaml` | arduino-cli build profiles (`nano_every` default, plus `uno`, `nano`, `mega`) pinned to board cores and the vendored libraries |
| `libraries/NeoNextion/` | Vendored [NeoNextion](https://github.com/DanNixon/NeoNextion) 2.2.0, the Nextion driver (GPL v2) |
| `libraries/AccelStepper/` | Vendored [AccelStepper](http://www.airspayce.com/mikem/arduino/AccelStepper/) 1.64 for future stepper control (GPL v3) |
| `build.sh` | Wrapper for Git Bash / macOS / Linux: compile, upload, serial monitor, port discovery |
| `.github/workflows/compile.yml` | CI: compiles the sketch for all four profiles on every push and PR |

The Nextion Editor project (`.HMI`) is not checked in yet. When it is, put it
under `hmi/` alongside the compiled `.tft` so the firmware and screen layout
version together.

## Hardware

- **Arduino Nano Every** is the primary target (ATmega4809, 5 V logic). It
  has a hardware `Serial1` on D0/D1 that is independent of the USB port, so
  the display gets a real UART and the serial monitor stays free for debug
  output. The sketch detects this at compile time via `HAVE_HWSERIAL1`.
- Also supported: **Uno** and classic **Nano** (SoftwareSerial on D10/D11,
  less reliable) and **Mega 2560** (hardware `Serial1` on pins 19/18).
- A Nextion display (any Basic/Enhanced/Intelligent model). Default baud is
  9600. The display's TX is 3.3 V and its RX tolerates 5 V, so no level
  shifting is needed with any of these boards.
- Optional: an LED on D7 that mirrors the UP button.

### Wiring diagram (Nano Every)

```
            Arduino Nano Every                                Nextion display
           +------------------+                              (4-pin JST lead)
  USB  <-->| USB              |                              +--------------+
           |                  |                              |              |
           |   D1 / TX1       |----------------------------->| RX   yellow  |
           |   D0 / RX1       |<-----------------------------| TX   blue    |
           |   5V             |------------------------------| 5V   red     |
           |   GND            |---------+--------------------| GND  black   |
           |                  |         |                    +--------------+
           |   D7             |--[330R]-|>|--+               
           |                  |             |                 optional status LED
           |   GND            |-------------+                 (anode to resistor)
           +------------------+
```

Cross the data lines: the display's **TX** goes to the Arduino's **RX**, and
vice versa. Getting this backwards is the single most common wiring fault
and shows up as `Nextion init: no reply`.

Pin table for every supported board:

| Nextion wire | Nano Every | Uno / Nano | Mega 2560 |
| --- | --- | --- | --- |
| red, 5V | 5V | 5V | 5V |
| black, GND | GND | GND | GND |
| blue, TX | D0 (RX1) | D10 | pin 19 (RX1) |
| yellow, RX | D1 (TX1) | D11 | pin 18 (TX1) |
| status LED (optional) | D7 | D7 | D7 |

Power the display from the Arduino's 5 V pin only for small panels (the 2.4"
to 3.5" Basic models draw well under 250 mA). Larger or Enhanced/Intelligent
panels can draw more than USB supplies; feed them from a separate 5 V source
and tie the grounds together.

On the Nano Every, D0 and D1 are dedicated to `Serial1` and are not shared
with USB, so uploading over USB works with the display connected. On an Uno
or Mega the USB port is `Serial`, which is separate from the pins used here,
so the same holds.

## Setting up a new machine

Everything builds with [arduino-cli](https://arduino.github.io/arduino-cli/).
The Arduino IDE is not required. All commands below are for Git Bash on
Windows and work unchanged on macOS and Linux.

```sh
./build.sh install     # winget on Windows, brew on macOS, curl installer on Linux
# open a new shell so arduino-cli is on PATH
./build.sh             # compiles for the default profile (nano_every)
```

The first compile downloads the pinned `arduino:avr` core into arduino-cli's
cache. Nothing is installed into a global sketchbook; the libraries come from
`libraries/` in this repo via the `dir:` entries in `sketch.yaml`.

The wrapper is thin. The underlying commands, if you prefer them directly:

```sh
arduino-cli compile --profile nano_every  Nextion_Tester
arduino-cli upload  --profile nano_every  -p COM7 Nextion_Tester   # or /dev/ttyACM0
arduino-cli monitor -p COM7 --config baudrate=115200
```

Profiles: `nano_every` (default), `uno`, `nano`, `mega`.

## Build, flash, monitor

```sh
./build.sh ports                  # list serial ports and detected boards
./build.sh build -p mega          # compile only, for the mega profile
./build.sh upload                 # compile and upload, auto-detect the port
./build.sh upload -P COM7         # compile and upload to a specific port
./build.sh upload --raw-dump      # raw byte dump build, see Troubleshooting
./build.sh monitor -P COM7        # 115200 baud debug console
./build.sh all -P COM7            # compile, upload, then monitor
./build.sh --help                 # all options
```

Expected output on the monitor after reset:

```
Nextion init: OK
Callbacks attached. Touch the buttons.
UP pressed
UP released
```

## Configuring the HMI in the Nextion Editor

The sketch expects, on page 0:

| Widget | Component id | objname |
| --- | --- | --- |
| UP button | 6 | `b1` |
| DOWN button | 7 | `b2` |

For **every** button you want the Arduino to hear about:

1. Select the button and read its `id` from the attribute pane. Mirror it in
   the `ID_*` defines at the top of the sketch. The `objname` is only used for
   writing to the widget (`setText`), the `id` is what identifies touch events.
2. Open the **Touch Press Event** tab and tick **Send Component ID**.
3. Open the **Touch Release Event** tab and tick **Send Component ID**.
4. Leave the page's `bauds` at 9600, or change `NEXTION_BAUD` in the sketch.

If a button is missing the Send Component ID tick, the display will never
send a touch frame for it and no amount of Arduino-side debugging will help.

## Why events were not registering (and how to avoid it again)

The original tester had the right idea but two things in `loop()` broke
event delivery. Both are worth understanding because they are easy to
reintroduce.

**1. Something other than `nex.poll()` was reading the display's serial port.**
The loop did `nextionSerial.read()` to print one incoming byte and then
called `nex.poll()`. A touch event arrives as seven bytes:

```
65  <page>  <component>  <01 press | 00 release>  FF FF FF
```

NeoNextion's `poll()` only recognises an event when it sees the `0x65`
header byte. The debug read consumed exactly that header, so `poll()` saw
`00 06 01 FF FF FF`, did not recognise it, and threw it away. Every event was
lost. The fix is a rule: **only `Nextion::poll()` may read from the display's
port.** If you need to look at raw bytes, set `RAW_DUMP 1` in the sketch,
which disables event decoding entirely while you inspect the stream.

**2. `getText()` was being called every loop iteration.** `getText` sends
`get b1.txt` and then blocks for up to 500 ms waiting for a `0x70` string
reply, discarding every byte that is not part of that reply. A touch frame
that lands during that window is eaten. Polling a widget's text in a tight
loop is never needed; the Arduino is the one setting it.

A third, smaller issue: `delay(300)` in the loop. SoftwareSerial has a 64 byte
buffer, so a press and release (14 bytes) survive 300 ms at 9600 baud, but a
few quick taps will overflow it and corrupt frames. `loop()` should call
`nex.poll()` and nothing that blocks.

## Troubleshooting

Work through these in order.

1. **`Nextion init: no reply`.** Wiring or baud. Swap TX and RX (the most
   common mistake), confirm 9600 in the HMI, confirm the display has power and
   shows page 0.
2. **Init OK but no events.** Build the raw dump variant with
   `./build.sh upload --raw-dump` (or set `RAW_DUMP 1` in the sketch) and press
   a button. If you see nothing, Send Component ID is not ticked in the HMI (see
   above). If you see `65 00 06 01 FF FF FF`, the display is fine; check that
   the page and component ids in the sketch match the second and third bytes.
3. **Events for one button but not the other.** Component id mismatch, or
   Send Component ID is ticked on Press but not Release (or vice versa).
4. **Garbage bytes or intermittent events on an Uno or classic Nano.**
   SoftwareSerial is bit-banged and gets disturbed by other interrupts. Use a
   Nano Every or Mega so the display is on a hardware `Serial1`, or drop the
   display baud rate.
5. **`setText` does nothing.** The `objname` in the sketch does not match the
   HMI, or the widget is on a page that is not currently shown.

## Vendored libraries and licensing

The two libraries under `libraries/` are copied into the repo rather than
installed from the Arduino library manager so that a clone always builds
against known versions. Do not edit them in place; if a fix is needed, bump
the version from upstream or document the local patch here.

- NeoNextion 2.2.0 by Dan Nixon, GPL v2. See `libraries/NeoNextion/LICENSE`.
  Known quirks: the API takes `char *` rather than `const char *`, so string
  literals need a `(char *)` cast to compile cleanly, and `receiveNumber`
  shifts bytes by 24 bits through a 16-bit `int` on AVR, so numeric reads
  above 65535 are wrong. Neither affects touch events.
- AccelStepper 1.64 by Mike McCauley, GPL v3 (commercial licence available
  from the author). See `libraries/AccelStepper/LICENSE`.

The project's own code is MIT (see `LICENSE`). Be aware that a compiled
firmware image that links both GPL libraries is subject to GPL terms when
distributed as a binary.

## Roadmap

- [ ] Check in the Nextion Editor `.HMI` and compiled `.tft` under `hmi/`
- [ ] Add the real pendant widgets: air blast and coolant dual-state buttons,
      status text fields, tool number display
- [ ] Define the ATC sequence (spindle stop, drawbar release, tool pick/place,
      drawbar clamp) and the interlocks around it
- [ ] Decide on the controller link to the CNC (relays, gcode over serial, or
      direct I/O) and document it
- [ ] Stepper-driven dust brush lift using the vendored AccelStepper
- [ ] Hardware test fixture or mock so CI can exercise the event path, not
      just compilation
