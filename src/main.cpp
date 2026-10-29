#include <Arduino.h>

enum GameState {
    READY,
    WAITING,
    REACT
};

GameState state = READY;

unsigned long waitUntil = 0;
unsigned long reactionStartedAt = 0;
unsigned long bestTime = 0;

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
    Serial.println("Press Enter to arm the game.");
}

void setup() {
    Serial.begin(9600);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // A0 is unconnected, so its electrical noise gives us
    // a somewhat unpredictable starting value.
    randomSeed(analogRead(A0));

    Serial.println();
    Serial.println("======================");
    Serial.println("    SAKURA REFLEX");
    Serial.println("======================");

    showReadyMessage();
}

void loop() {
    bool pressed = enterWasPressed();
    unsigned long now = millis();

    if (state == READY) {
        if (pressed) {
            Serial.println("Get ready...");

            digitalWrite(LED_BUILTIN, LOW);

            // Wait somewhere between 2 and 5 seconds.
            waitUntil = now + random(2000, 5001);
            state = WAITING;
        }
    }

    else if (state == WAITING) {
        if (pressed) {
            Serial.println("Too soon! You anticipated the light.");
            state = READY;
            showReadyMessage();
        }

        else if (now >= waitUntil) {
            digitalWrite(LED_BUILTIN, HIGH);

            reactionStartedAt = millis();
            state = REACT;

            Serial.println("NOW! Press Enter!");
        }
    }

    else if (state == REACT) {
        if (pressed) {
            unsigned long reactionTime =
                millis() - reactionStartedAt;

            digitalWrite(LED_BUILTIN, LOW);

            Serial.print("Reaction time: ");
            Serial.print(reactionTime);
            Serial.println(" ms");

            if (bestTime == 0 || reactionTime < bestTime) {
                bestTime = reactionTime;
                Serial.println("New best time!");
            } else {
                Serial.print("Best time: ");
                Serial.print(bestTime);
                Serial.println(" ms");
            }

            state = READY;
            showReadyMessage();
        }

        else if (now - reactionStartedAt >= 10000) {
            digitalWrite(LED_BUILTIN, LOW);

            Serial.println("You fell asleep. No score!");

            state = READY;
            showReadyMessage();
        }
    }
}