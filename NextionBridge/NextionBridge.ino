/*
 * NextionBridge
 *
 * Turns the Arduino into a transparent USB-to-display serial bridge so the
 * Nextion Editor (or any TFT upload tool) can program the display through
 * the Arduino's USB port. No USB-TTL adapter or microSD card needed.
 *
 * How to use:
 *   1. ./build.sh upload -s NextionBridge -P COMx
 *   2. In the Nextion Editor: File > TFT file output is NOT needed; use
 *      Upload, pick the Arduino's COM port, pick a baud rate, Go.
 *      The Editor connects at the display's current baud (DISPLAY_BAUD,
 *      9600 by default, it will scan if you picked something else), then
 *      sends "whmi-wri <size>,<baud>,0". This sketch watches for that
 *      command and switches both serial ports to the requested baud so
 *      the bulk transfer runs at full speed.
 *   3. When the display reboots into the new firmware, re-flash the real
 *      sketch: ./build.sh upload -P COMx
 *
 * Also handy as a plain serial console to the display: open a monitor at
 * DISPLAY_BAUD and type Nextion commands (the Editor's instruction set).
 * Terminate commands with three 0xFF bytes, or set the monitor to send
 * "\xFF\xFF\xFF" line endings if it supports it.
 *
 * Boards without a hardware Serial1 (Uno, classic Nano) fall back to
 * SoftwareSerial, which is not reliable above ~57600. Keep the upload baud
 * at 9600 or 19200 on those, or use a Nano Every / Mega.
 *
 * Supports upload protocol v1.0 ("whmi-wri") and v1.2 ("whmi-wris").
 */

#ifndef DISPLAY_BAUD
#define DISPLAY_BAUD 9600 // must match the display's current `bauds` setting
#endif

#define NEXTION_RX_PIN 10
#define NEXTION_TX_PIN 11

#if defined(HAVE_HWSERIAL1)
#define display Serial1
#else
#include <SoftwareSerial.h>
SoftwareSerial display(NEXTION_RX_PIN, NEXTION_TX_PIN);
#endif

// ---------------------------------------------------------------------------
// Detects "whmi-wri[s] <size>,<baud>,<x>\xFF\xFF\xFF" in the host->display
// stream and reports the baud once the terminator has been forwarded.
// ---------------------------------------------------------------------------

class UploadCommandWatcher
{
public:
  // Feed every host->display byte. Returns a non-zero baud exactly once,
  // after the full command (including FF FF FF) has been seen.
  uint32_t feed(uint8_t c)
  {
    switch (m_state)
    {
    case MATCH:
      if (c == (uint8_t)kPrefix[m_pos])
      {
        m_pos++;
        if (kPrefix[m_pos] == '\0')
          m_state = OPT_S;
      }
      else
        reset(c);
      return 0;

    case OPT_S:
      if (c == 's')
        return 0;
      if (c == ' ')
        m_state = SIZE;
      else
        reset(c);
      return 0;

    case SIZE:
      if (c == ',')
      {
        m_baud = 0;
        m_state = BAUD;
      }
      else if (c < '0' || c > '9')
        reset(c);
      return 0;

    case BAUD:
      if (c >= '0' && c <= '9')
        m_baud = m_baud * 10 + (c - '0');
      else if (c == ',')
      {
        m_ff = 0;
        m_state = TAIL;
      }
      else
        reset(c);
      return 0;

    case TAIL:
      if (c == 0xFF)
      {
        if (++m_ff == 3)
        {
          uint32_t b = m_baud;
          reset(0);
          return b;
        }
      }
      else
        m_ff = 0;
      return 0;
    }
    return 0;
  }

private:
  enum State
  {
    MATCH,
    OPT_S,
    SIZE,
    BAUD,
    TAIL
  };

  static constexpr const char *kPrefix = "whmi-wri";

  void reset(uint8_t c)
  {
    m_state = MATCH;
    m_pos = 0;
    // The mismatching byte might itself be the start of a command.
    if (c == (uint8_t)kPrefix[0])
      m_pos = 1;
  }

  State m_state = MATCH;
  uint8_t m_pos = 0;
  uint8_t m_ff = 0;
  uint32_t m_baud = 0;
};

UploadCommandWatcher watcher;

void setup()
{
  Serial.begin(DISPLAY_BAUD);
  display.begin(DISPLAY_BAUD);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
  // Host -> display
  while (Serial.available() > 0)
  {
    uint8_t c = Serial.read();
    display.write(c);

    uint32_t newBaud = watcher.feed(c);
    if (newBaud != 0)
    {
      // Let the command finish leaving the wire before changing speed.
      display.flush();
      Serial.flush();
      display.begin(newBaud);
      Serial.begin(newBaud);
      digitalWrite(LED_BUILTIN, HIGH); // on = upload in progress
    }
  }

  // Display -> host
  while (display.available() > 0)
    Serial.write(display.read());
}
