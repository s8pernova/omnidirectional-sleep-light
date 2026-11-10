#include <Arduino.h>
#include <ESP32Servo.h>
#include <RTClib.h>

namespace Pins
{
    constexpr uint8_t PAN_SERVO = 18;
    constexpr uint8_t TILT_SERVO = 19;
    constexpr uint8_t LAMP = 27;
    constexpr uint8_t ARM_SWITCH = 32;
    constexpr uint8_t LIGHT_SENSOR = 34;
} // namespace Pins

namespace Motion
{
    constexpr uint8_t HOME_PAN = 90;
    constexpr uint8_t HOME_TILT = 90;
    constexpr uint8_t WAKE_PAN = 45;
    constexpr uint8_t WAKE_TILT = 60;
    constexpr unsigned long STEP_INTERVAL_MS = 20;
} // namespace Motion

constexpr unsigned long FADE_DURATION_MS = 5000;

Servo panServo;
Servo tiltServo;
RTC_DS1307 rtc;

enum class State : uint8_t
{
    DISARMED,
    READY,
    POSITIONING,
    FADING,
    ACTIVE,
    FAULT
};

State state = State::FAULT;
uint8_t currentPan = Motion::HOME_PAN;
uint8_t currentTilt = Motion::HOME_TILT;
uint8_t lampBrightness = 0;
unsigned long lastMotionStepAt = 0;
unsigned long fadeStartedAt = 0;

const __FlashStringHelper *stateName(State value)
{
    switch (value)
    {
    case State::DISARMED:
        return F("DISARMED");
    case State::READY:
        return F("READY");
    case State::POSITIONING:
        return F("POSITIONING");
    case State::FADING:
        return F("FADING");
    case State::ACTIVE:
        return F("ACTIVE");
    case State::FAULT:
        return F("FAULT");
    }

    return F("UNKNOWN");
}

void setState(State next)
{
    if (state == next)
    {
        return;
    }

    state = next;
    Serial.print(F("[State] "));
    Serial.println(stateName(state));
}

bool isArmed()
{
    // INPUT_PULLUP makes the grounded switch position read LOW.
    return digitalRead(Pins::ARM_SWITCH) == LOW;
}

void setLamp(uint8_t brightness)
{
    lampBrightness = brightness;
    analogWrite(Pins::LAMP, lampBrightness);
}

void stopWakeSequence()
{
    setLamp(0);

    // A hardware fault is latched until the board is reset and startup passes.
    if (state == State::FAULT)
    {
        return;
    }

    setState(isArmed() ? State::READY : State::DISARMED);
}

void startWakeSequence()
{
    if (!isArmed())
    {
        Serial.println(F("[Wake] Ignored: switch is disarmed."));
        return;
    }

    if (state == State::FAULT)
    {
        Serial.println(F("[Wake] Ignored: system is in FAULT."));
        return;
    }

    setLamp(0);
    lastMotionStepAt = 0;
    setState(State::POSITIONING);
}

uint8_t moveOneDegreeToward(uint8_t current, uint8_t target)
{
    if (current < target)
    {
        return current + 1;
    }
    if (current > target)
    {
        return current - 1;
    }
    return current;
}

void updatePositioning()
{
    const unsigned long now = millis();
    if (now - lastMotionStepAt < Motion::STEP_INTERVAL_MS)
    {
        return;
    }
    lastMotionStepAt = now;

    currentPan = moveOneDegreeToward(currentPan, Motion::WAKE_PAN);
    currentTilt = moveOneDegreeToward(currentTilt, Motion::WAKE_TILT);
    panServo.write(currentPan);
    tiltServo.write(currentTilt);

    if (currentPan == Motion::WAKE_PAN && currentTilt == Motion::WAKE_TILT)
    {
        fadeStartedAt = now;
        setState(State::FADING);
    }
}

void updateFade()
{
    const unsigned long elapsed = millis() - fadeStartedAt;
    if (elapsed >= FADE_DURATION_MS)
    {
        setLamp(255);
        setState(State::ACTIVE);
        return;
    }

    setLamp(static_cast<uint8_t>((elapsed * 255UL) / FADE_DURATION_MS));
}

void printTwoDigits(uint8_t value)
{
    if (value < 10)
    {
        Serial.print('0');
    }
    Serial.print(value);
}

void printStatus()
{
    Serial.print(F("[Status] state="));
    Serial.print(stateName(state));
    Serial.print(F(" switch="));
    Serial.print(isArmed() ? F("ARMED") : F("DISARMED"));
    Serial.print(F(" pan="));
    Serial.print(currentPan);
    Serial.print(F(" tilt="));
    Serial.print(currentTilt);
    Serial.print(F(" lamp="));
    Serial.print(lampBrightness);
    Serial.print(F(" lightSensor="));
    Serial.print(analogRead(Pins::LIGHT_SENSOR));

    if (state != State::FAULT)
    {
        const DateTime now = rtc.now();
        Serial.print(F(" time="));
        printTwoDigits(now.hour());
        Serial.print(':');
        printTwoDigits(now.minute());
        Serial.print(':');
        printTwoDigits(now.second());
    }

    Serial.println();
}

void printHelp()
{
    Serial.println(F("Commands:"));
    Serial.println(F("  t  run a test wake sequence"));
    Serial.println(F("  x  cancel the sequence and turn off the lamp"));
    Serial.println(F("  s  print system status"));
    Serial.println(F("  h  print this help"));
}

void handleSerial()
{
    while (Serial.available() > 0)
    {
        const char command = static_cast<char>(Serial.read());

        switch (command)
        {
        case 't':
        case 'T':
            startWakeSequence();
            break;
        case 'x':
        case 'X':
            stopWakeSequence();
            break;
        case 's':
        case 'S':
            printStatus();
            break;
        case 'h':
        case 'H':
            printHelp();
            break;
        case '\r':
        case '\n':
        case ' ':
            break;
        default:
            Serial.print(F("[Serial] Unknown command: "));
            Serial.println(command);
            break;
        }
    }
}

void setup()
{
    Serial.begin(9600);

    pinMode(Pins::ARM_SWITCH, INPUT_PULLUP);
    pinMode(Pins::LAMP, OUTPUT);
    setLamp(0);

    Serial.println(F("\nOmnidirectional Sleep Light"));

    if (!rtc.begin())
    {
        Serial.println(F("[Fault] DS1307 was not detected."));
        setState(State::FAULT);
    }
    else if (!rtc.isrunning())
    {
        Serial.println(F("[Fault] DS1307 clock is not running."));
        setState(State::FAULT);
    }
    else
    {
        // Do not energize either motor until all required startup checks pass.
        panServo.attach(Pins::PAN_SERVO);
        tiltServo.attach(Pins::TILT_SERVO);
        panServo.write(currentPan);
        tiltServo.write(currentTilt);

        setState(isArmed() ? State::READY : State::DISARMED);
    }

    printHelp();
    printStatus();
}

void loop()
{
    handleSerial();

    if (state != State::FAULT)
    {
        if (!isArmed() && state != State::DISARMED)
        {
            stopWakeSequence();
        }
        else if (isArmed() && state == State::DISARMED)
        {
            setState(State::READY);
        }
    }

    if (state == State::POSITIONING)
    {
        updatePositioning();
    }
    else if (state == State::FADING)
    {
        updateFade();
    }
}
