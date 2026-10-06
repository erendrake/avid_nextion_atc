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
| `Nextion_Tester/sketch.yaml` | arduino-cli build profiles (`uno`, `nano`, `mega`) pinned to a board core and the vendored libraries |
| `libraries/NeoNextion/` | Vendored [NeoNextion](https://github.com/DanNixon/NeoNextion) 2.2.0, the Nextion driver (GPL v2) |
| `libraries/AccelStepper/` | Vendored [AccelStepper](http://www.airspayce.com/mikem/arduino/AccelStepper/) 1.64 for future stepper control (GPL v3) |
| `build.ps1` | Windows wrapper: compile, upload, serial monitor, port discovery |
| `.github/workflows/compile.yml` | CI: compiles the sketch for Uno, Nano and Mega on every push and PR |

The Nextion Editor project (`.HMI`) is not checked in yet. When it is, put it
under `hmi/` alongside the compiled `.tft` so the firmware and screen layout
version together.

## Hardware

- Any 5 V AVR Arduino. An **Uno** or **Nano** works with SoftwareSerial on
  pins 10 and 11. A **Mega 2560** (or Leonardo/Micro) is preferred because the
  sketch automatically switches to the hardware `Serial1` UART, which is more
  reliable and leaves USB free for debug output.
- A Nextion display (any Basic/Enhanced/Intelligent model). Default baud is
  9600.
- Optional: an LED on pin 7 that mirrors the UP button.

Wiring, display to Arduino:

| Nextion wire | Uno / Nano | Mega / Leonardo |
| --- | --- | --- |
| red, 5V | 5V | 5V |
| black, GND | GND | GND |
| blue, TX | D10 | RX1 (pin 19 on Mega) |
| yellow, RX | D11 | TX1 (pin 18 on Mega) |

Power the display from the Arduino's 5 V pin only for small displays. Larger
panels draw more than USB can supply; feed them from a separate 5 V source
and share ground.

## Setting up a new machine

Everything builds with [arduino-cli](https://arduino.github.io/arduino-cli/).
The Arduino IDE is not required.

```powershell
winget install --id ArduinoSA.CLI -e     # or: .\build.ps1 install
# open a new terminal so arduino-cli is on PATH
.\build.ps1                               # compiles for the Uno profile
```

The first compile downloads the pinned `arduino:avr` core into arduino-cli's
cache. Nothing is installed into a global sketchbook; the libraries come from
`libraries/` in this repo via the `dir:` entries in `sketch.yaml`.

On macOS or Linux, call arduino-cli directly:

```sh
arduino-cli compile --profile uno  Nextion_Tester
arduino-cli upload  --profile uno  -p /dev/ttyACM0 Nextion_Tester
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=115200
```

## Build, flash, monitor

```powershell
.\build.ps1 ports                 # list serial ports and detected boards
.\build.ps1 -Profile mega         # compile only
.\build.ps1 upload                # compile and upload, auto-detect the port
.\build.ps1 upload -Port COM7     # compile and upload to a specific port
.\build.ps1 upload -RawDump       # raw byte dump build, see Troubleshooting
.\build.ps1 monitor -Port COM7    # 115200 baud debug console
.\build.ps1 all -Port COM7        # compile, upload, then monitor
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
   `.\build.ps1 upload -RawDump` (or set `RAW_DUMP 1` in the sketch) and press
   a button. If you see nothing, Send Component ID is not ticked in the HMI (see
   above). If you see `65 00 06 01 FF FF FF`, the display is fine; check that
   the page and component ids in the sketch match the second and third bytes.
3. **Events for one button but not the other.** Component id mismatch, or
   Send Component ID is ticked on Press but not Release (or vice versa).
4. **Garbage bytes or intermittent events on an Uno.** SoftwareSerial is
   bit-banged and gets disturbed by other interrupts. Move to a Mega and use
   `Serial1`, or drop the display baud rate.
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
