#include <Servo.h>

Servo panServo;
Servo tiltServo;

void setup() {
    panServo.attach(9);
    panServo.write(90); // Set initial position to 90 degrees
}

void loop() {
    // Pass
}
