#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(2000);
}

void loop() {
  Serial.println("HELLO");
  delay(500);
}
