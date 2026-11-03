#include <Arduino.h>
#include <Servo.h>

Servo panServo;
Servo tiltServo;

void setup() {
    panServo.attach(9);
}

void loop() {
    Serial.begin(9600);

    static int count = 0;
    count += 15; // Increment count and wrap around at 180
    
    int position = count % 180; // Ensure position stays within 0-179
    
    Serial.print("[Servo] ");
    Serial.println(position);

    panServo.write(position);

    delay(1000);
}
