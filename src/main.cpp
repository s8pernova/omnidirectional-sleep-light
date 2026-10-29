#include <Arduino.h>

void setup() {
    Serial.begin(9600);
}

void loop() {
    static int count = 0;
    Serial.print("Hello, World! ");
    Serial.println(count);
    count++;
    delay(1000);
}