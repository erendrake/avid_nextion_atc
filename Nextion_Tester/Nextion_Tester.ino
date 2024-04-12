#include <Nextion.h>
#include <NextionButton.h>
#include <NextionDualStateButton.h>
#include <NextionText.h>
#include <NextionPage.h>
#include <SoftwareSerial.h>

#define displayRx 10
#define displayTx 11
#define statusLedPin 7
#define NEXTION_PORT Serial1
int incomingByte = 0;

SoftwareSerial nextionSerial(displayRx, displayTx);  // RX, TX

Nextion nex(nextionSerial);
NextionButton upButton(nex, 0, 6, "b1");
NextionButton downButton(nex, 0, 7, "b2");

//NextionDualStateButton airButton(nex, 0, 2, "airButton");
//NextionDualStateButton collantButton(nex, 0, 3, "coolantButton");
//NextionText avidText(nex, 0, 1, "avidText");
//NextionText brushText(nex, 0, 4, "brushText");

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  nextionSerial.begin(9600);
  nex.init();
  pinMode(statusLedPin, OUTPUT);


  Serial.println(upButton.attachCallback(&upButtonCallback));
  // Serial.println(downButton.attachCallback(&downButtonCallback));

  char upBuffer[50];
  upButton.getText(upBuffer, 50);
  Serial.println(upBuffer);
  upButton.setText("PU");
  //brushText.setText("PU");
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);  
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  //Serial.println("loop");  
  if(nextionSerial.available() > 0){    
    Serial.println("available");   
    // read the incoming byte:
    incomingByte = nextionSerial.read();

    // say what you got:
    Serial.print("I received: ");
    Serial.println(incomingByte, HEX); 
  }
  nex.poll();
  delay(300);
  char upBuffer[50];
  upButton.getText(upBuffer, 50);
  //Serial.println(upBuffer);
  //downButton.setText("PU");

}


void upButtonCallback(NextionEventType type, INextionTouchable *widget) {
  Serial.println("UP BUTTON PRESSED!");  
  if (type == NEX_EVENT_PUSH)
  {
    digitalWrite(statusLedPin, HIGH);
    upButton.setText("Hyah!");
  }
  else if (type == NEX_EVENT_POP)
  {
    digitalWrite(statusLedPin, LOW);
    upButton.setText("You pressed me");
  }
}
// void downButtonCallback(NextionEventType type, INextionTouchable *widget) {
//   Serial.println("downCallback");
// }
