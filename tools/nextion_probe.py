#!/usr/bin/env python3
"""Nextion serial diagnostics through the Arduino. Works on macOS, Linux, Windows.

Two modes:

  connect (default)
      Expects the NextionBridge sketch on the Arduino. Sends the Nextion
      "connect" handshake plus a few harmless queries and prints every reply
      as hex and ASCII. A reply starting with "comok" proves the wiring and
      the baud rate in one shot. Silence at every baud means the display's
      TX line or power is the problem, not configuration.

  listen
      Prints whatever the Arduino sends for N seconds. Use it with the
      RAW_DUMP build of Nextion_Tester (115200 baud) and press buttons on
      the display. With --decode, touch frames are parsed into
      "page / component / PRESS|RELEASE" lines so component ids can be read
      straight off the hardware.

Examples:
  python3 tools/nextion_probe.py                              # connect, 9600, auto-detect port
  python3 tools/nextion_probe.py -p /dev/cu.usbmodem14101 -b 115200
  python3 tools/nextion_probe.py listen -b 115200 -t 45 --decode

Needs pyserial:  python3 -m pip install --user pyserial
"""

import argparse
import re
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("pyserial is not installed. Run: python3 -m pip install --user pyserial", file=sys.stderr)
    sys.exit(2)

ARDUINO_VIDS = {0x2341, 0x2A03}
TERM = b"\xff\xff\xff"


def find_port():
    ports = list(list_ports.comports())
    arduino = [p for p in ports if p.vid in ARDUINO_VIDS]
    if arduino:
        return arduino[0].device
    usb = [p for p in ports if "usbmodem" in p.device or "usbserial" in p.device or "ttyACM" in p.device]
    if usb:
        return usb[0].device
    sys.exit("No Arduino found. Plug it in or pass -p <port>. Known ports: "
             + ", ".join(p.device for p in ports) if ports else "No serial ports at all.")


def open_port(port, baud):
    ser = serial.Serial(port, baud, timeout=0.05)
    # Toggle DTR to reset the board, then wait for the sketch to boot.
    ser.dtr = False
    time.sleep(0.1)
    ser.dtr = True
    time.sleep(1.5)
    ser.reset_input_buffer()
    return ser


def read_for(ser, seconds):
    end = time.monotonic() + seconds
    buf = bytearray()
    while time.monotonic() < end:
        chunk = ser.read(256)
        if chunk:
            buf.extend(chunk)
    return bytes(buf)


def show(label, data):
    hexs = " ".join(f"{b:02X}" for b in data)
    asc = "".join(chr(b) if 32 <= b < 127 else "." for b in data)
    print(f"{label:<10} {len(data):3d} bytes | {hexs} | {asc}")


def mode_connect(args):
    port = args.port or find_port()
    print(f"port={port} baud={args.baud}")
    ser = open_port(port, args.baud)
    try:
        def cmd(name, text, wait):
            ser.write(text.encode("ascii") + TERM)
            show(name, read_for(ser, wait))

        cmd("flush", "", 0.3)
        cmd("connect", "connect", 1.2)
        cmd("bkcmd=3", "bkcmd=3", 0.6)
        cmd("sendme", "sendme", 0.6)
        cmd("bauds", "get bauds", 0.6)
    finally:
        ser.close()


FRAME_RE = re.compile(
    r"\b65 ([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2}) FF FF FF\b"
    r"|\b88 FF FF FF\b"
    r"|\b66 ([0-9A-F]{2}) FF FF FF\b"
    r"|\b00 00 00 FF FF FF\b"
)


def decode_frames(text, seen):
    """Scan a hex-token stream for Nextion frames. Returns new human lines."""
    out = []
    for m in FRAME_RE.finditer(text):
        if m.start() in seen:
            continue
        seen.add(m.start())
        if m.group(1) is not None:
            page, comp, ev = int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16)
            kind = "PRESS" if ev == 1 else "RELEASE" if ev == 0 else f"event {ev}"
            out.append(f"touch: page {page}, component id {comp}, {kind}")
        elif m.group(0).startswith("88"):
            out.append("display ready (0x88): it just booted")
        elif m.group(4) is not None:
            out.append(f"current page id {int(m.group(4), 16)}")
        else:
            out.append("display startup message (00 00 00)")
    return out


def mode_listen(args):
    port = args.port or find_port()
    print(f"port={port} baud={args.baud} listening {args.time}s (Ctrl+C to stop)")
    ser = open_port(port, args.baud)
    text = ""
    seen = set()
    end = time.monotonic() + args.time
    try:
        while time.monotonic() < end:
            chunk = ser.read(256)
            if not chunk:
                continue
            s = chunk.decode("latin-1")
            sys.stdout.write(s)
            sys.stdout.flush()
            if args.decode:
                text += s.upper()
                for line in decode_frames(text, seen):
                    print(f"\n  >> {line}")
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
    if args.decode:
        ids = sorted({int(m.group(2), 16) for m in FRAME_RE.finditer(text) if m.group(1) is not None})
        print(f"\n\ncomponent ids seen: {ids if ids else 'none'}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", nargs="?", default="connect", choices=["connect", "listen"])
    ap.add_argument("-p", "--port", help="serial port (default: first Arduino found)")
    ap.add_argument("-b", "--baud", type=int, default=9600,
                    help="baud (default 9600 for connect; use 115200 for listen with the RAW_DUMP build)")
    ap.add_argument("-t", "--time", type=float, default=30, help="listen seconds (default 30)")
    ap.add_argument("--decode", action="store_true", help="listen: parse touch frames into readable lines")
    args = ap.parse_args()
    if args.mode == "connect":
        mode_connect(args)
    else:
        mode_listen(args)


if __name__ == "__main__":
    main()
