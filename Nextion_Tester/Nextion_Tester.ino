/*
 * Nextion_Tester
 *
 * Bring-up sketch for the AVID ATC pendant: a Nextion HMI display talking to
 * an Arduino over serial. Every button in the HMI gets its own widget object
 * and its own callback. A press lights LED_BUILTIN for LED_FLASH_MS and logs
 * which button it was, so a misrouted event is obvious on the serial monitor.
 *
 * Wiring (display -> Arduino), see README.md for the full diagram:
 *   red    5V   -> 5V
 *   black  GND  -> GND
 *   blue   TX   -> Serial1 RX (D0 on Nano Every, 19 on Mega) or NEXTION_RX_PIN
 *   yellow RX   -> Serial1 TX (D1 on Nano Every, 18 on Mega) or NEXTION_TX_PIN
 *
 * Nextion Editor checklist for every button you want events from:
 *   - note the component's "id" and "objname" attributes and mirror them below
 *   - tick "Send Component ID" on the Touch Press Event tab
 *   - tick "Send Component ID" on the Touch Release Event tab
 *   - leave bauds at 9600 (or change NEXTION_BAUD to match)
 *
 * To add a button, do three things (search for "ADD A BUTTON"):
 *   1. #define its component id
 *   2. declare a NextionButton for it
 *   3. write its callback and attach it in setup()
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
// Can also be set without editing: ./build.sh upload --raw-dump
#ifndef RAW_DUMP
#define RAW_DUMP 0
#endif

#define DEBUG_BAUD 115200
#define NEXTION_BAUD 9600

// How long LED_BUILTIN stays lit after any button press. Pressing again
// inside the window restarts it.
#define LED_FLASH_MS 1000

// Pins used only when the board has no spare hardware UART (Uno, Nano).
#define NEXTION_RX_PIN 10 // Arduino RX  <- display TX (blue)
#define NEXTION_TX_PIN 11 // Arduino TX  -> display RX (yellow)

// Page and component IDs as shown in the Nextion Editor attribute pane.
// ADD A BUTTON (1/3): define its id here.
#define PAGE_MAIN 0
#define ID_UP_BUTTON 6
#define ID_DOWN_BUTTON 7

// ---------------------------------------------------------------------------
// Serial port selection
// ---------------------------------------------------------------------------

#if defined(HAVE_HWSERIAL1)
// Nano Every (D0/D1), Mega (19/18), Leonardo, Micro: use the real UART. Far
// more reliable than SoftwareSerial and leaves the USB port free for debug.
#define nextionSerial Serial1
#else
#include <SoftwareSerial.h>
SoftwareSerial nextionSerial(NEXTION_RX_PIN, NEXTION_TX_PIN);
#endif

// ---------------------------------------------------------------------------
// Widgets: one object per button. The page and component id are what the
// library matches incoming touch frames against; the name is only used for
// outgoing commands.
// ---------------------------------------------------------------------------

Nextion nex(nextionSerial);

// ADD A BUTTON (2/3): declare it here.
NextionButton upButton(nex, PAGE_MAIN, ID_UP_BUTTON, "b1");
NextionButton downButton(nex, PAGE_MAIN, ID_DOWN_BUTTON, "b2");

// ADD A BUTTON (3/3): declare its callback here, define it at the bottom,
// and attach it in setup(). Each button has its own function on purpose; do
// not route several buttons through one handler.
void onUpButton(NextionEventType type, INextionTouchable *widget);
void onDownButton(NextionEventType type, INextionTouchable *widget);

// ---------------------------------------------------------------------------
// LED flash timer. Non-blocking: loop() must keep calling nex.poll(), so no
// delay() anywhere after setup().
// ---------------------------------------------------------------------------

static bool ledOn = false;
static unsigned long ledOnSince = 0;

static void flashLed()
{
  digitalWrite(LED_BUILTIN, HIGH);
  ledOn = true;
  ledOnSince = millis();
}

static void serviceLed()
{
  if (ledOn && (millis() - ledOnSince) >= LED_FLASH_MS)
  {
    digitalWrite(LED_BUILTIN, LOW);
    ledOn = false;
  }
}

// ---------------------------------------------------------------------------

void setup()
{
  Serial.begin(DEBUG_BAUD);
  nextionSerial.begin(NEXTION_BAUD);
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
  Serial.println(F("Callbacks attached: b1 (id 6) -> onUpButton, b2 (id 7) -> onDownButton"));
  Serial.println(F("Press a button: LED lights for 1 s and the button is named here."));
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

  serviceLed();
}

// ---------------------------------------------------------------------------
// Callbacks. One per button. Only the press (PUSH) edge lights the LED; the
// release (POP) edge is logged so you can see both halves of the event.
// ---------------------------------------------------------------------------

void onUpButton(NextionEventType type, INextionTouchable *widget)
{
  (void)widget;
  if (type == NEX_EVENT_PUSH)
  {
    Serial.println(F("UP   (b1, id 6) pressed"));
    flashLed();
  }
  else if (type == NEX_EVENT_POP)
  {
    Serial.println(F("UP   (b1, id 6) released"));
  }
}

void onDownButton(NextionEventType type, INextionTouchable *widget)
{
  (void)widget;
  if (type == NEX_EVENT_PUSH)
  {
    Serial.println(F("DOWN (b2, id 7) pressed"));
    flashLed();
  }
  else if (type == NEX_EVENT_POP)
  {
    Serial.println(F("DOWN (b2, id 7) released"));
  }
}
