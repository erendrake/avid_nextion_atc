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
| `NextionBridge/NextionBridge.ino` | USB-to-display serial pass-through so the Nextion Editor can upload `.tft` files through the Arduino |
| `libraries/NeoNextion/` | Vendored [NeoNextion](https://github.com/DanNixon/NeoNextion) 2.2.0, the Nextion driver (GPL v2) |
| `libraries/AccelStepper/` | Vendored [AccelStepper](http://www.airspayce.com/mikem/arduino/AccelStepper/) 1.64 for future stepper control (GPL v3) |
| `build.sh` | Wrapper for Git Bash / macOS / Linux: compile, upload, serial monitor, port discovery |
| `tools/nextion_probe.py` | Serial diagnostics: `connect` handshake through the bridge, or decode touch frames from the raw-dump build |
| `tools/baud_scan.sh` | Reflashes the bridge at each common baud and probes, to find the display's rate |
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
- No other parts. Button presses are shown on the board's built-in LED
  (D13 on the Nano Every) and on the serial monitor.

### Wiring diagram (Nano Every)

```
            Arduino Nano Every                                Nextion display
           +------------------+                              (4-pin JST lead)
  USB  <-->| USB              |                              +--------------+
           |                  |                              |              |
           |   D1 / TX1       |----------------------------->| RX   yellow  |
           |   D0 / RX1       |<-----------------------------| TX   blue    |
           |   5V             |------------------------------| 5V   red     |
           |   GND            |------------------------------| GND  black   |
           |                  |                              +--------------+
           |   D13 (LED)      |  built-in LED: lights 1 s on any button press
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

### macOS notes

- `brew install arduino-cli` is all the toolchain needs. The Nano Every
  needs no driver.
- The board appears as `/dev/cu.usbmodemXXXX`. Use the `cu.` device, not
  the `tty.` twin. `./build.sh ports` lists it, and auto-detection skips the
  `Bluetooth-Incoming-Port` entry.
- The diagnostics in `tools/` need Python 3 with pyserial:
  `python3 -m pip install --user pyserial` (or
  `python3 -m pip install --user -r tools/requirements.txt`). If pip refuses
  with an "externally managed environment" error, use a venv:
  `python3 -m venv .venv && .venv/bin/pip install pyserial` and run the
  tools with `.venv/bin/python`.

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
Callbacks attached: b1 (id 6) -> onUpButton, b2 (id 7) -> onDownButton
Press a button: LED lights for 1 s and the button is named here.
UP   (b1, id 6) pressed
UP   (b1, id 6) released
DOWN (b2, id 7) pressed
DOWN (b2, id 7) released
```

The built-in LED lights for one second on every press. The log line names
the button, so if the wrong name shows up for a physical button the
component id in the sketch does not match the HMI.

## Uploading

There are two things to program: the Arduino and the display. They are
independent, and the display keeps its firmware across Arduino re-flashes.

### Arduino

```sh
./build.sh ports                  # find the COM port
./build.sh upload -P COMx         # compile + flash Nextion_Tester
```

If `ports` shows nothing but `COM1`, Windows has not enumerated the board.
Try another USB cable first: many micro-USB cables are charge-only and the
Nano Every will light up on them but never appear as a port. Then try
another USB port. The Nano Every needs no driver on Windows 10/11.

### Display

The display is programmed with a `.tft` file produced by the Nextion Editor
(`File > TFT file output`). Three ways to get it onto the panel, in order of
preference:

**1. microSD card (simplest, no extra hardware).**

1. Format a microSD card (32 GB or smaller) as FAT32.
2. Copy exactly one `.tft` file onto it, at the root. Remove any old ones.
3. With the display **powered off**, insert the card.
4. Power the display on. It shows an "update" progress screen, then
   `Update Successed!`.
5. Power off, remove the card, power on. The new HMI is running.

If it boots to the old screen without updating, the card is not FAT32, has
more than one `.tft`, or is larger than 32 GB.

**2. Through the Arduino with the bridge sketch (no adapter, no card).**

`NextionBridge` turns the Arduino into a transparent USB-to-display serial
pass-through. The Nextion Editor's upload protocol starts at the display's
current baud and then asks for a faster one; the bridge watches for that
request and switches both ports so the bulk transfer runs at full speed.

```sh
./build.sh upload -s NextionBridge -P COMx     # flash the bridge
# Nextion Editor: Upload > pick the Arduino's COM port > Go
./build.sh upload -P COMx                      # flash the real sketch again
```

Use a Nano Every or Mega for this. On an Uno or classic Nano the display is
on SoftwareSerial, which cannot keep up with the upload baud; keep the
Editor's baud at 9600 or 19200 there, or use the microSD card.

This path is written against the published upload protocol but has not
yet been exercised on hardware; if the Editor reports a connection failure,
fall back to the card.

**3. USB-TTL adapter directly to the display.** Wire a 5 V-tolerant USB-TTL
adapter (TX to the display's blue wire, RX to yellow, plus 5 V and GND),
unplug the display from the Arduino, and use the Editor's Upload with the
adapter's COM port. This is what Nextion documents officially.

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

### Adding a button

Every button has its own widget object and its own callback function. There
is deliberately no shared "any button" handler: the library matches each
incoming touch frame to exactly one widget by page and component id, and a
dedicated callback makes a wrong id show up as the wrong name in the log.
The three places to edit are marked `ADD A BUTTON` in the sketch:

```cpp
#define ID_AIR_BUTTON 8                                   // 1. id from the Editor
NextionButton airButton(nex, PAGE_MAIN, ID_AIR_BUTTON, "b3");   // 2. widget
void onAirButton(NextionEventType type, INextionTouchable *widget)   // 3. callback
{
  (void)widget;
  if (type == NEX_EVENT_PUSH) { Serial.println(F("AIR (b3, id 8) pressed")); flashLed(); }
}
// ...and in setup():  airButton.attachCallback(&onAirButton);
```

Dual-state buttons use `NextionDualStateButton` with the same constructor
and callback signature.

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

## Diagnostics

Two tools in `tools/` answer the two questions that come up most, without
the Nextion Editor and from any OS. Both need Python 3 with pyserial and
auto-detect the Arduino's port (pass `-p <port>` to override).

**Is the display wired and at what baud?** Flash the bridge and send the
`connect` handshake. A `comok` reply proves TX, RX, power and baud at once.

```sh
./build.sh upload -s NextionBridge
python3 tools/nextion_probe.py                 # connect at 9600
python3 tools/nextion_probe.py -b 115200       # or another rate
tools/baud_scan.sh                             # reflash + probe at every common rate
```

Silence at every rate means the display's TX line (blue) is not reaching
the Arduino, or the display has no power. It is not a configuration issue.

**What are my component ids?** Flash the raw-dump build, listen, and press
each button. `--decode` turns the frames into readable lines and lists the
ids seen at the end.

```sh
./build.sh upload --raw-dump
python3 tools/nextion_probe.py listen -b 115200 -t 45 --decode
```

```
65 00 06 01 FF FF FF
  >> touch: page 0, component id 6, PRESS
65 00 06 00 FF FF FF
  >> touch: page 0, component id 6, RELEASE

component ids seen: [6, 7]
```

Put those ids into the `ID_*` defines in the sketch, flash the normal build
with `./build.sh upload`, and the callbacks will fire.

## Troubleshooting

Work through these in order.

1. **`Nextion init: no reply`.** Wiring, power or baud. Run the `connect`
   probe (see Diagnostics). If it is silent at every baud, check the display
   is lit and inspect the blue TX wire end to end; a broken TX lead produced
   exactly this on the bench. Otherwise swap TX and RX, the most common
   mistake.
2. **Init OK but no events.** Build the raw dump variant with
   `./build.sh upload --raw-dump` (or set `RAW_DUMP 1` in the sketch), run
   `python3 tools/nextion_probe.py listen -b 115200 --decode`, and press a
   button. If you see nothing, Send Component ID is not ticked in the HMI (see
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
