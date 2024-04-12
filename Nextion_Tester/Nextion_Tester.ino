#include <Nextion.h>
#include <NextionButton.h>

#define blink_time 200
Nextion nex(Serial1);
NextionButton upButton(nex, 0, 6, "b1");

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial1.begin(9600);
  nex.init();
  pinMode(LED_BUILTIN, OUTPUT);


  Serial.println(upButton.attachCallback(&upButtonCallback));
  // Serial.println(downButton.attachCallback(&downButtonCallback));

  char upBuffer[50];
  upButton.getText(upBuffer, 50);
  Serial.println(upBuffer);
  upButton.setText("PU");
}

void loop() {  
  char upBuffer[50];
  upButton.getText(upBuffer, 50);
  Serial.println(upBuffer);  

  upButton.setText("PU");

  delay(blink_time);  
  digitalWrite(LED_BUILTIN, HIGH);
  delay(blink_time);  
  digitalWrite(LED_BUILTIN, LOW);
}


void upButtonCallback(NextionEventType type, INextionTouchable *widget) {
  Serial.println("UP BUTTON PRESSED!");  
  if (type == NEX_EVENT_PUSH)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    upButton.setText("Hyah!");
  }
  else if (type == NEX_EVENT_POP)
  {
    digitalWrite(LED_BUILTIN, LOW);
    upButton.setText("You pressed me");
  }
}
// void downButtonCallback(NextionEventType type, INextionTouchable *widget) {
//   Serial.println("downCallback");
// }
