/*
 * Nextion_Tester
 *
 * Bring-up sketch for the AVID ATC pendant: a Nextion HMI display talking to
 * an Arduino over serial. It proves that touch events from the display reach
 * the Arduino and that the Arduino can write back to widgets.
 *
 * Wiring (display -> Arduino):
 *   red    5V   -> 5V
 *   black  GND  -> GND
 *   blue   TX   -> NEXTION_RX_PIN (or Serial1 RX on boards that have it)
 *   yellow RX   -> NEXTION_TX_PIN (or Serial1 TX on boards that have it)
 *
 * Nextion Editor checklist for every button you want events from:
 *   - note the component's "id" and "objname" attributes and mirror them below
 *   - tick "Send Component ID" on the Touch Press Event tab
 *   - tick "Send Component ID" on the Touch Release Event tab
 *   - leave bauds at 9600 (or change NEXTION_BAUD to match)
 *
 * IMPORTANT: Nextion::poll() must be the only reader of the display's serial
 * port. Reading even one byte elsewhere strips the 0x65 touch-event header and
 * the library silently drops the event. Use RAW_DUMP below to inspect bytes.
 */

#include <Nextion.h>
#include <NextionButton.h>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// 1 = print every raw byte from the display as hex and do NOT decode events.
// Use this to confirm wiring and that "Send Component ID" is enabled. A press
// followed by a release on page 0, component 6 should print:
//   65 00 06 01 FF FF FF
//   65 00 06 00 FF FF FF
// Can also be set without editing: .\build.ps1 upload -RawDump
#ifndef RAW_DUMP
#define RAW_DUMP 0
#endif

#define DEBUG_BAUD 115200
#define NEXTION_BAUD 9600

#define STATUS_LED_PIN 7

// Pins used only when the board has no spare hardware UART (Uno, Nano).
#define NEXTION_RX_PIN 10 // Arduino RX  <- display TX (blue)
#define NEXTION_TX_PIN 11 // Arduino TX  -> display RX (yellow)

// Page and component IDs as shown in the Nextion Editor attribute pane.
#define PAGE_MAIN 0
#define ID_UP_BUTTON 6
#define ID_DOWN_BUTTON 7

// ---------------------------------------------------------------------------
// Serial port selection
// ---------------------------------------------------------------------------

#if defined(HAVE_HWSERIAL1)
// Mega, Leonardo, Micro, etc.: use the real UART. Far more reliable than
// SoftwareSerial and leaves the USB port free for debug output.
#define nextionSerial Serial1
#else
#include <SoftwareSerial.h>
SoftwareSerial nextionSerial(NEXTION_RX_PIN, NEXTION_TX_PIN);
#endif

// ---------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------

Nextion nex(nextionSerial);
NextionButton upButton(nex, PAGE_MAIN, ID_UP_BUTTON, "b1");
NextionButton downButton(nex, PAGE_MAIN, ID_DOWN_BUTTON, "b2");

// Future widgets, once they exist in the HMI with matching ids:
// NextionDualStateButton airButton(nex, PAGE_MAIN, 2, "airButton");
// NextionDualStateButton coolantButton(nex, PAGE_MAIN, 3, "coolantButton");
// NextionText avidText(nex, PAGE_MAIN, 1, "avidText");
// NextionText brushText(nex, PAGE_MAIN, 4, "brushText");

void onUpButton(NextionEventType type, INextionTouchable *widget);
void onDownButton(NextionEventType type, INextionTouchable *widget);

// ---------------------------------------------------------------------------

void setup()
{
  Serial.begin(DEBUG_BAUD);
  nextionSerial.begin(NEXTION_BAUD);

  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  // Give the display time to finish booting before we talk to it.
  delay(500);

  bool ok = nex.init();
  Serial.print(F("Nextion init: "));
  Serial.println(ok ? F("OK") : F("no reply (check wiring, baud, power)"));

#if RAW_DUMP
  Serial.println(F("RAW_DUMP mode: printing display bytes, events not decoded"));
#else
  upButton.attachCallback(&onUpButton);
  downButton.attachCallback(&onDownButton);

  upButton.setText((char *)"UP");
  downButton.setText((char *)"DOWN");
  Serial.println(F("Callbacks attached. Touch the buttons."));
#endif

  // Heartbeat so you can tell setup() finished even without a serial monitor.
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
}

void loop()
{
#if RAW_DUMP
  while (nextionSerial.available() > 0)
  {
    uint8_t b = nextionSerial.read();
    if (b < 0x10)
      Serial.print('0');
    Serial.print(b, HEX);
    Serial.print(' ');
  }
#else
  // Drain the port and dispatch touch events to the attached callbacks.
  // Do not add delay() here and do not read nextionSerial anywhere else.
  nex.poll();
#endif
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void onUpButton(NextionEventType type, INextionTouchable *widget)
{
  (void)widget;
  if (type == NEX_EVENT_PUSH)
  {
    Serial.println(F("UP pressed"));
    digitalWrite(STATUS_LED_PIN, HIGH);
    upButton.setText((char *)"UP!");
  }
  else if (type == NEX_EVENT_POP)
  {
    Serial.println(F("UP released"));
    digitalWrite(STATUS_LED_PIN, LOW);
    upButton.setText((char *)"UP");
  }
}

void onDownButton(NextionEventType type, INextionTouchable *widget)
{
  (void)widget;
  if (type == NEX_EVENT_PUSH)
  {
    Serial.println(F("DOWN pressed"));
    downButton.setText((char *)"DOWN!");
  }
  else if (type == NEX_EVENT_POP)
  {
    Serial.println(F("DOWN released"));
    downButton.setText((char *)"DOWN");
  }
}
