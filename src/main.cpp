#include <Arduino.h>
#include <Servo.h>

Servo panServo;
Servo tiltServo;

void setup() {
    Serial.begin(9600);
    panServo.attach(9);
}

void loop() {
    static int position = 0;

    position = (position + 15) % 180;

    Serial.print("[Servo] ");
    Serial.println(position);

    panServo.write(position);

    delay(1000);
}