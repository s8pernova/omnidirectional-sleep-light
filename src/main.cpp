#include <Arduino.h>

bool enterWasPressed() {
    bool pressed = false;

    while (Serial.available() > 0) {
        char character = Serial.read();

        if (character == '\n' || character == '\r') {
            pressed = true;
        }
    }

    return pressed;
}

void showReadyMessage() {
    Serial.println();
    Serial.println("Press Enter to start the game.");
}

void setup() {
    Serial.begin(9600);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    randomSeed(analogRead(A0));

    Serial.println();
    Serial.println("================================");
    Serial.println("          GAME START!");
    Serial.println("================================");
}

void loop() {
    bool pressed = wasEnterPressed();

    
}