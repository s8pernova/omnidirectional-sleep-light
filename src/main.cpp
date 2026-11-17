#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>

namespace Pins
{
    // XIAO ESP32-S3 exposed pins; these names match diagram.json.
    constexpr uint8_t PAN_SERVO = D0;
    constexpr uint8_t TILT_SERVO = D1;
    constexpr uint8_t LAMP = D3;
    constexpr uint8_t RTC_SDA = D4;
    constexpr uint8_t RTC_SCL = D5;
    constexpr uint8_t ARM_SWITCH = D7;
} // namespace Pins

namespace Motion
{
    // Servo angles are expressed in degrees
    constexpr uint8_t HOME_PAN = 90;
    constexpr uint8_t HOME_TILT = 90;
    constexpr uint8_t WAKE_PAN = 45;
    constexpr uint8_t WAKE_TILT = 60;
    constexpr unsigned long STEP_INTERVAL_MS = 20;
} // namespace Motion

namespace Pwm
{
    constexpr uint32_t SERVO_FREQUENCY_HZ = 50;
    constexpr uint8_t SERVO_RESOLUTION_BITS = 14;
    constexpr uint32_t SERVO_PERIOD_US = 1000000UL / SERVO_FREQUENCY_HZ;
    constexpr uint16_t SERVO_MIN_PULSE_US = 544;
    constexpr uint16_t SERVO_MAX_PULSE_US = 2400;
    constexpr uint8_t PAN_SERVO_CHANNEL = 0;
    constexpr uint8_t TILT_SERVO_CHANNEL = 1;

    constexpr uint32_t LAMP_FREQUENCY_HZ = 5000;
    constexpr uint8_t LAMP_RESOLUTION_BITS = 8;
    constexpr uint8_t LAMP_CHANNEL = 2;
} // namespace Pwm

constexpr unsigned long FADE_DURATION_MS = 5000;
constexpr unsigned long ARM_SWITCH_DEBOUNCE_MS = 30;

RTC_DS1307 rtc;

enum class State : uint8_t
{
    // Wake sequence: READY -> POSITIONING -> FADING -> ACTIVE
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
bool rawArmSwitchState = false;
bool debouncedArmSwitchState = false;
unsigned long armSwitchChangedAt = 0;

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
    // Log transitions once instead of repeatedly printing the current state
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
    return debouncedArmSwitchState;
}

bool readArmSwitch()
{
    // INPUT_PULLUP makes the grounded switch position read LOW.
    return digitalRead(Pins::ARM_SWITCH) == LOW;
}

void initializeArmSwitch()
{
    pinMode(Pins::ARM_SWITCH, INPUT_PULLUP);
    rawArmSwitchState = readArmSwitch();
    debouncedArmSwitchState = rawArmSwitchState;
    armSwitchChangedAt = millis();
}

void updateArmSwitch()
{
    const bool currentReading = readArmSwitch();
    const unsigned long now = millis();

    if (currentReading != rawArmSwitchState)
    {
        rawArmSwitchState = currentReading;
        armSwitchChangedAt = now;
        return;
    }

    // Accept the new position only after it remains stable long enough for
    // mechanical contact bounce to finish.
    if (currentReading != debouncedArmSwitchState &&
        now - armSwitchChangedAt >= ARM_SWITCH_DEBOUNCE_MS)
    {
        debouncedArmSwitchState = currentReading;
    }
}

void setLamp(uint8_t brightness)
{
    lampBrightness = brightness;
    ledcWrite(Pins::LAMP, lampBrightness);
}

uint32_t servoDutyForAngle(uint8_t angle)
{
    const uint32_t pulseWidthUs = map(
        angle,
        0,
        180,
        Pwm::SERVO_MIN_PULSE_US,
        Pwm::SERVO_MAX_PULSE_US);
    const uint32_t maximumDuty = (1UL << Pwm::SERVO_RESOLUTION_BITS) - 1;

    return (pulseWidthUs * maximumDuty) / Pwm::SERVO_PERIOD_US;
}

void writeServo(uint8_t pin, uint8_t angle)
{
    ledcWrite(pin, servoDutyForAngle(angle));
}

bool initializePwmOutputs()
{
    // Channels 0 and 1 share the same 50 Hz servo timer. The lamp uses a
    // separate channel/timer so its 5 kHz PWM cannot disturb either servo.
    const bool panAttached = ledcAttachChannel(
        Pins::PAN_SERVO,
        Pwm::SERVO_FREQUENCY_HZ,
        Pwm::SERVO_RESOLUTION_BITS,
        Pwm::PAN_SERVO_CHANNEL);
    const bool tiltAttached = ledcAttachChannel(
        Pins::TILT_SERVO,
        Pwm::SERVO_FREQUENCY_HZ,
        Pwm::SERVO_RESOLUTION_BITS,
        Pwm::TILT_SERVO_CHANNEL);
    const bool lampAttached = ledcAttachChannel(
        Pins::LAMP,
        Pwm::LAMP_FREQUENCY_HZ,
        Pwm::LAMP_RESOLUTION_BITS,
        Pwm::LAMP_CHANNEL);

    if (!panAttached || !tiltAttached || !lampAttached)
    {
        if (panAttached)
        {
            ledcDetach(Pins::PAN_SERVO);
        }
        if (tiltAttached)
        {
            ledcDetach(Pins::TILT_SERVO);
        }
        if (lampAttached)
        {
            ledcDetach(Pins::LAMP);
        }
        return false;
    }

    writeServo(Pins::PAN_SERVO, currentPan);
    writeServo(Pins::TILT_SERVO, currentTilt);
    setLamp(0);
    return true;
}

void stopWakeSequence()
{
    // A hardware fault is latched until the board is reset and startup passes
    if (state == State::FAULT)
    {
        return;
    }

    setLamp(0);
    setState(isArmed() ? State::READY : State::DISARMED);
}

void startWakeSequence()
{
    // Both the physical arm switch and a healthy startup state are required
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
    // Small increments keep both servos moving smoothly toward the target
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
    // Use millis() instead of delay() so serial input remains responsive
    const unsigned long now = millis();
    if (now - lastMotionStepAt < Motion::STEP_INTERVAL_MS)
    {
        return;
    }
    lastMotionStepAt = now;

    currentPan = moveOneDegreeToward(currentPan, Motion::WAKE_PAN);
    currentTilt = moveOneDegreeToward(currentTilt, Motion::WAKE_TILT);
    writeServo(Pins::PAN_SERVO, currentPan);
    writeServo(Pins::TILT_SERVO, currentTilt);

    if (currentPan == Motion::WAKE_PAN && currentTilt == Motion::WAKE_TILT)
    {
        // Start timing the fade only after both axes reach the wake position
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

    // Linearly scale elapsed time to the full 8-bit brightness range
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
    // Drain every pending character so commands do not accumulate in the buffer
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

    // Select the XIAO's exposed I2C pins explicitly
    Wire.begin(Pins::RTC_SDA, Pins::RTC_SCL);

    initializeArmSwitch();
    pinMode(Pins::LAMP, OUTPUT);
    digitalWrite(Pins::LAMP, LOW);
    lampBrightness = 0;

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
    else if (!initializePwmOutputs())
    {
        Serial.println(F("[Fault] PWM outputs could not be initialized."));
        setState(State::FAULT);
    }
    else
    {
        setState(isArmed() ? State::READY : State::DISARMED);
    }

    printHelp();
    printStatus();
}

void loop()
{
    updateArmSwitch();
    handleSerial();

    // The physical switch always takes priority over an in-progress sequence
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

    // Advance time dependent states without blocking the main loop
    if (state == State::POSITIONING)
    {
        updatePositioning();
    }
    else if (state == State::FADING)
    {
        updateFade();
    }
}
