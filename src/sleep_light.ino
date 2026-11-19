// Omnidirectional Sleep Light - ESP32 DevKit (ESP32-WROOM-32) firmware
//
// Hardware: ESP32 DevKit, 4x4 membrane keypad, 16x2 I2C LCD (PCF8574 backpack),
// DS3231 or DS1307 RTC module, 2x SG90 servos (pan/tilt), LED lamp on a 9 V
// battery switched low-side by an RFP30N06LE logic-level MOSFET.
// Wiring: docs/wiring-esp32dev.md
//
// Keypad:
//   0-9  type the alarm time as HHMM (24 h) when the screen asks for it
//   A    stop the light (lamp off)
//   B    enter a new alarm time
//   C    aim mode: 4/6 = pan, 2/8 = tilt, 5 = save, C or # = leave without saving
//   D    test the full wake sequence now
//   *    arm / disarm the alarm
//
// Serial (9600 baud): TYYYY-MM-DD HH:MM:SS sets the RTC, 's' prints status.

#include <Arduino.h>
#include <ESP32Servo.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <RTClib.h>
#include <Wire.h>
#include <math.h>

// Set to 1 if your RTC module is a DS1307 instead of a DS3231.
#define USE_DS1307 0

// =====================================================================
// PINS (ESP32 DevKit GPIO numbers)
// =====================================================================
namespace Pins
{
    constexpr uint8_t I2C_SDA = 21; // LCD + RTC
    constexpr uint8_t I2C_SCL = 22; // LCD + RTC
    constexpr uint8_t PAN_SERVO = 18;
    constexpr uint8_t TILT_SERVO = 19;
    constexpr uint8_t LAMP_GATE = 23; // MOSFET gate (through 220 ohm)

    // Keypad ribbon, left to right with the keys facing you: R1 R2 R3 R4 C1 C2 C3 C4
    constexpr uint8_t KEY_ROWS[4] = {32, 33, 25, 26};
    constexpr uint8_t KEY_COLS[4] = {27, 14, 13, 4};
} // namespace Pins

// =====================================================================
// SETTINGS
// =====================================================================
constexpr unsigned long FADE_DURATION_MS = 20000UL; // 0 -> 100 % in 20 s
constexpr float FADE_CURVE = 4.0f;                   // higher = darker start, steeper end
constexpr unsigned long FADE_UPDATE_MS = 10UL;

constexpr uint32_t LAMP_PWM_HZ = 10000;  // above the range where drivers tend to whine
constexpr uint8_t LAMP_PWM_BITS = 12;     // 4096 steps so the dim start is smooth
constexpr uint32_t LAMP_PWM_MAX = (1UL << LAMP_PWM_BITS) - 1;

constexpr uint8_t PAN_MIN = 0, PAN_MAX = 180;
constexpr uint8_t TILT_MIN = 20, TILT_MAX = 160; // keep the lamp off its hard stops
constexpr uint8_t DEFAULT_WAKE_PAN = 45;
constexpr uint8_t DEFAULT_WAKE_TILT = 60;
constexpr uint8_t AIM_STEP_DEG = 5;
constexpr unsigned long SERVO_STEP_MS = 20; // 1 degree per 20 ms = slow and quiet
constexpr unsigned long SERVO_SETTLE_MS = 400;
constexpr bool DETACH_SERVOS_WHEN_IDLE = true; // stops SG90 hum/jitter overnight

// Set true for ONE upload to copy the computer's clock into the RTC, then set false.
constexpr bool SYNC_RTC_ON_UPLOAD = false;

// =====================================================================
// HARDWARE OBJECTS
// =====================================================================
#if USE_DS1307
RTC_DS1307 rtc;
#else
RTC_DS3231 rtc;
#endif

const char KEYMAP[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};
byte rowPins[4] = {Pins::KEY_ROWS[0], Pins::KEY_ROWS[1], Pins::KEY_ROWS[2], Pins::KEY_ROWS[3]};
byte colPins[4] = {Pins::KEY_COLS[0], Pins::KEY_COLS[1], Pins::KEY_COLS[2], Pins::KEY_COLS[3]};
Keypad keypad(makeKeymap(KEYMAP), rowPins, colPins, 4, 4);

LiquidCrystal_I2C *lcd = nullptr; // created after the I2C address is detected
Servo panServo;
Servo tiltServo;
ESP32PWM lampPwm;
Preferences prefs;

// =====================================================================
// STATE
// =====================================================================
enum class State : uint8_t
{
    ENTERING_TIME, // typing HHMM
    IDLE,          // showing the clock, waiting for the alarm
    AIMING,        // adjusting the wake direction
    POSITIONING,   // servos moving to the wake direction
    FADING,        // 20 s exponential sunrise
    LIGHT_ON,      // full brightness until A is pressed
    FAULT          // RTC missing: lamp off, no motion
};

State state = State::FAULT;

uint8_t alarmHour = 7;
uint8_t alarmMinute = 0;
bool alarmArmed = false;
uint8_t wakePan = DEFAULT_WAKE_PAN;
uint8_t wakeTilt = DEFAULT_WAKE_TILT;

uint8_t currentPan = DEFAULT_WAKE_PAN;
uint8_t currentTilt = DEFAULT_WAKE_TILT;
uint8_t aimPan = DEFAULT_WAKE_PAN;
uint8_t aimTilt = DEFAULT_WAKE_TILT;
bool servosAttached = false;

uint8_t entryDigits[4] = {0, 0, 0, 0};
uint8_t entryCount = 0;

uint32_t lastTriggerKey = 0; // YYYYMMDDHHMM of the last trigger, prevents re-triggering
unsigned long lastStepAt = 0;
unsigned long fadeStartedAt = 0;
unsigned long lastFadeUpdateAt = 0;
unsigned long lastScreenAt = 0;
float lampLevel = 0.0f; // 0.0 - 1.0

// =====================================================================
// LCD HELPERS
// =====================================================================
void lcdLine(uint8_t row, const char *text)
{
    if (!lcd)
        return;
    char padded[17];
    snprintf(padded, sizeof(padded), "%-16.16s", text);
    lcd->setCursor(0, row);
    lcd->print(padded);
}

void showScreen(const char *line1, const char *line2)
{
    lcdLine(0, line1);
    lcdLine(1, line2);
}

uint8_t findLcdAddress()
{
    const uint8_t candidates[] = {0x27, 0x3F, 0x20, 0x38};
    for (uint8_t addr : candidates)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
            return addr;
    }
    return 0;
}

void showIdleScreen()
{
    const DateTime now = rtc.now();
    char l1[24], l2[24];
    snprintf(l1, sizeof(l1), "Now   %02d:%02d:%02d", now.hour(), now.minute(), now.second());
    snprintf(l2, sizeof(l2), "Alarm %02d:%02d %s", alarmHour, alarmMinute, alarmArmed ? "ON" : "OFF");
    showScreen(l1, l2);
}

void showEntryScreen()
{
    char typed[6] = "__:__";
    const uint8_t map[4] = {0, 1, 3, 4};
    for (uint8_t i = 0; i < entryCount; i++)
        typed[map[i]] = '0' + entryDigits[i];
    char l2[17];
    snprintf(l2, sizeof(l2), "%s  B:clear", typed);
    showScreen("Alarm time HHMM", l2);
}

void showAimScreen()
{
    char l1[17];
    snprintf(l1, sizeof(l1), "Pan %3u Tilt %3u", aimPan, aimTilt);
    showScreen(l1, "4/6 2/8 5:save");
}

// =====================================================================
// LAMP
// =====================================================================
void setLamp(float level)
{
    lampLevel = constrain(level, 0.0f, 1.0f);
    lampPwm.write(static_cast<uint32_t>(lampLevel * LAMP_PWM_MAX + 0.5f));
}

// Normalized exponential: exactly 0 at t=0 and exactly 1 at t=FADE_DURATION.
float sunriseLevel(unsigned long elapsedMs)
{
    const float p = static_cast<float>(elapsedMs) / static_cast<float>(FADE_DURATION_MS);
    return (expf(FADE_CURVE * p) - 1.0f) / (expf(FADE_CURVE) - 1.0f);
}

// =====================================================================
// SERVOS
// =====================================================================
void attachServos()
{
    if (servosAttached)
        return;
    panServo.setPeriodHertz(50);
    tiltServo.setPeriodHertz(50);
    panServo.attach(Pins::PAN_SERVO, 500, 2400); // SG90 pulse range
    tiltServo.attach(Pins::TILT_SERVO, 500, 2400);
    panServo.write(currentPan);
    tiltServo.write(currentTilt);
    servosAttached = true;
}

void detachServos()
{
    if (!servosAttached || !DETACH_SERVOS_WHEN_IDLE)
        return;
    delay(SERVO_SETTLE_MS); // let the horn finish moving before releasing it
    panServo.detach();
    tiltServo.detach();
    servosAttached = false;
}

// Moves pan first, then tilt, one degree per step, so only one servo draws
// current at a time. Returns true once both axes are at the target.
bool stepTowards(uint8_t targetPan, uint8_t targetTilt)
{
    const unsigned long now = millis();
    if (now - lastStepAt < SERVO_STEP_MS)
        return false;
    lastStepAt = now;

    if (currentPan != targetPan)
    {
        currentPan += (currentPan < targetPan) ? 1 : -1;
        panServo.write(currentPan);
        return false;
    }
    if (currentTilt != targetTilt)
    {
        currentTilt += (currentTilt < targetTilt) ? 1 : -1;
        tiltServo.write(currentTilt);
        return false;
    }
    return true;
}

// =====================================================================
// PERSISTENT SETTINGS (survive reset / power loss)
// =====================================================================
void loadSettings()
{
    prefs.begin("sleeplight", true);
    alarmHour = prefs.getUChar("hour", 7);
    alarmMinute = prefs.getUChar("minute", 0);
    alarmArmed = prefs.getBool("armed", false);
    wakePan = constrain(prefs.getUChar("pan", DEFAULT_WAKE_PAN), PAN_MIN, PAN_MAX);
    wakeTilt = constrain(prefs.getUChar("tilt", DEFAULT_WAKE_TILT), TILT_MIN, TILT_MAX);
    const bool hasAlarm = prefs.isKey("hour");
    prefs.end();

    if (alarmHour > 23 || alarmMinute > 59)
    {
        alarmHour = 7;
        alarmMinute = 0;
        alarmArmed = false;
    }
    if (!hasAlarm)
        alarmArmed = false;
}

void saveAlarm()
{
    prefs.begin("sleeplight", false);
    prefs.putUChar("hour", alarmHour);
    prefs.putUChar("minute", alarmMinute);
    prefs.putBool("armed", alarmArmed);
    prefs.end();
}

void saveAim()
{
    prefs.begin("sleeplight", false);
    prefs.putUChar("pan", wakePan);
    prefs.putUChar("tilt", wakeTilt);
    prefs.end();
}

// =====================================================================
// STATE TRANSITIONS
// =====================================================================
const char *stateName(State s)
{
    switch (s)
    {
    case State::ENTERING_TIME: return "ENTERING_TIME";
    case State::IDLE: return "IDLE";
    case State::AIMING: return "AIMING";
    case State::POSITIONING: return "POSITIONING";
    case State::FADING: return "FADING";
    case State::LIGHT_ON: return "LIGHT_ON";
    case State::FAULT: return "FAULT";
    }
    return "?";
}

void setState(State next)
{
    if (state == next)
        return;
    state = next;
    Serial.printf("[State] %s\n", stateName(state));
}

void goIdle()
{
    setLamp(0);
    detachServos();
    setState(State::IDLE);
    showIdleScreen();
}

void startEntry()
{
    setLamp(0);
    detachServos();
    entryCount = 0;
    setState(State::ENTERING_TIME);
    showEntryScreen();
}

void startWake(const char *reason)
{
    Serial.printf("[Wake] %s\n", reason);
    setLamp(0);
    attachServos();
    lastStepAt = 0;
    setState(State::POSITIONING);
    showScreen("Good morning!", "Aiming...  A:off");
}

void stopLight()
{
    Serial.println(F("[Wake] Light off"));
    goIdle();
}

void startAiming()
{
    aimPan = wakePan;
    aimTilt = wakeTilt;
    attachServos();
    setState(State::AIMING);
    showAimScreen();
}

// =====================================================================
// INPUT
// =====================================================================
void handleEntryKey(char key)
{
    if (key < '0' || key > '9' || entryCount >= 4)
        return;
    entryDigits[entryCount++] = key - '0';
    showEntryScreen();
    if (entryCount < 4)
        return;

    const uint8_t h = entryDigits[0] * 10 + entryDigits[1];
    const uint8_t m = entryDigits[2] * 10 + entryDigits[3];
    if (h > 23 || m > 59)
    {
        showScreen("Invalid time!", "Try again");
        delay(1200);
        startEntry();
        return;
    }
    alarmHour = h;
    alarmMinute = m;
    alarmArmed = true;
    lastTriggerKey = 0;
    saveAlarm();
    Serial.printf("[Alarm] Set for %02u:%02u (armed)\n", h, m);
    goIdle();
}

void handleAimKey(char key)
{
    switch (key)
    {
    case '4': aimPan = (aimPan >= PAN_MIN + AIM_STEP_DEG) ? aimPan - AIM_STEP_DEG : PAN_MIN; break;
    case '6': aimPan = min<int>(aimPan + AIM_STEP_DEG, PAN_MAX); break;
    case '8': aimTilt = (aimTilt >= TILT_MIN + AIM_STEP_DEG) ? aimTilt - AIM_STEP_DEG : TILT_MIN; break;
    case '2': aimTilt = min<int>(aimTilt + AIM_STEP_DEG, TILT_MAX); break;
    case '5':
        wakePan = aimPan;
        wakeTilt = aimTilt;
        saveAim();
        Serial.printf("[Aim] Saved pan=%u tilt=%u\n", wakePan, wakeTilt);
        showScreen("Direction saved", "");
        delay(800);
        goIdle();
        return;
    case 'C':
    case '#':
        goIdle();
        return;
    default:
        return;
    }
    // Faint light while aiming so you can see where it points.
    setLamp(0.05f);
    showAimScreen();
}

void handleKey(char key)
{
    Serial.printf("[Key] %c\n", key);

    if (state == State::FAULT)
        return;

    // A always turns the light off, whatever is happening.
    if (key == 'A')
    {
        if (state == State::POSITIONING || state == State::FADING || state == State::LIGHT_ON)
            stopLight();
        else if (state == State::AIMING)
            goIdle();
        return;
    }

    switch (state)
    {
    case State::ENTERING_TIME:
        if (key == 'B')
            startEntry();
        else if (key == '#' || key == '*')
            goIdle(); // leave without changing the alarm
        else
            handleEntryKey(key);
        break;
    case State::AIMING:
        handleAimKey(key);
        break;
    case State::IDLE:
        if (key == 'B')
            startEntry();
        else if (key == 'C')
            startAiming();
        else if (key == 'D')
            startWake("Test sequence");
        else if (key == '*')
        {
            alarmArmed = !alarmArmed;
            saveAlarm();
            Serial.printf("[Alarm] %s\n", alarmArmed ? "Armed" : "Disarmed");
            showIdleScreen();
        }
        break;
    default:
        break;
    }
}

void printStatus()
{
    const DateTime now = rtc.now();
    Serial.printf("[Status] state=%s time=%02d:%02d:%02d alarm=%02u:%02u %s wake=(%u,%u) lamp=%.1f%%\n",
                  stateName(state), now.hour(), now.minute(), now.second(), alarmHour, alarmMinute,
                  alarmArmed ? "ARMED" : "DISARMED", wakePan, wakeTilt, lampLevel * 100.0f);
}

// Accepts "TYYYY-MM-DD HH:MM:SS" to set the RTC, or single-letter commands.
void handleSerial()
{
    if (!Serial.available())
        return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
        return;

    if (line.charAt(0) == 'T')
    {
        int y, mo, d, h, mi, s;
        if (sscanf(line.c_str(), "T%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6)
        {
            const DateTime t(y, mo, d, h, mi, s);
            if (t.isValid() && y >= 2000 && y <= 2099)
            {
                rtc.adjust(t);
                Serial.println(F("[RTC] Time set"));
                return;
            }
        }
        Serial.println(F("[RTC] Use: TYYYY-MM-DD HH:MM:SS"));
        return;
    }
    switch (line.charAt(0))
    {
    case 's': printStatus(); break;
    case 'd': handleKey('D'); break;
    case 'a': handleKey('A'); break;
    default: Serial.println(F("Commands: TYYYY-MM-DD HH:MM:SS, s=status, d=test, a=off")); break;
    }
}

// =====================================================================
// ALARM + SEQUENCE UPDATES
// =====================================================================
void checkAlarm()
{
    if (state != State::IDLE || !alarmArmed)
        return;
    const DateTime now = rtc.now();
    if (now.hour() != alarmHour || now.minute() != alarmMinute)
        return;
    const uint32_t key = (now.year() % 100) * 100000000UL + now.month() * 1000000UL + now.day() * 10000UL +
                         now.hour() * 100UL + now.minute();
    if (key == lastTriggerKey)
        return;
    lastTriggerKey = key;
    startWake("Alarm time reached");
}

void updateSequence()
{
    switch (state)
    {
    case State::POSITIONING:
        if (stepTowards(wakePan, wakeTilt))
        {
            detachServos(); // silent while the light fades in
            fadeStartedAt = millis();
            lastFadeUpdateAt = 0;
            setState(State::FADING);
            showScreen("Good morning!", "Sunrise... A:off");
        }
        break;

    case State::FADING:
    {
        const unsigned long elapsed = millis() - fadeStartedAt;
        if (elapsed >= FADE_DURATION_MS)
        {
            setLamp(1.0f);
            setState(State::LIGHT_ON);
            showScreen("Good morning!", "100%   A:off");
            break;
        }
        if (millis() - lastFadeUpdateAt >= FADE_UPDATE_MS)
        {
            lastFadeUpdateAt = millis();
            setLamp(sunriseLevel(elapsed));
        }
        break;
    }

    case State::AIMING:
        // Follow the aim target smoothly instead of jumping 5 degrees at once.
        stepTowards(aimPan, aimTilt);
        break;

    default:
        break;
    }
}

// =====================================================================
// SETUP / LOOP
// =====================================================================
bool rtcClockValid()
{
#if USE_DS1307
    return rtc.isrunning();
#else
    return !rtc.lostPower();
#endif
}

void setup()
{
    Serial.begin(9600);
    Serial.setTimeout(100);

    // Lamp first, so the MOSFET is held off as early as possible.
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    lampPwm.attachPin(Pins::LAMP_GATE, LAMP_PWM_HZ, LAMP_PWM_BITS);
    setLamp(0);

    Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);

    const uint8_t lcdAddr = findLcdAddress();
    if (lcdAddr)
    {
        lcd = new LiquidCrystal_I2C(lcdAddr, 16, 2);
        lcd->init();
        lcd->backlight();
        Serial.printf("[LCD] Found at 0x%02X\n", lcdAddr);
    }
    else
    {
        Serial.println(F("[LCD] Not found on I2C (check SDA/SCL/power)"));
    }
    showScreen("Sleep Light", "Starting...");

    if (!rtc.begin(&Wire))
    {
        Serial.println(F("[Fault] RTC not found on I2C"));
        showScreen("RTC not found", "Check wiring");
        setState(State::FAULT);
        return;
    }

    if (SYNC_RTC_ON_UPLOAD || !rtcClockValid())
    {
        // Compile time + a few seconds for the upload.
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)) + TimeSpan(0, 0, 0, 10));
        Serial.println(F("[RTC] Set from compile time. Use TYYYY-MM-DD HH:MM:SS to correct it."));
    }

    loadSettings();

    // Park the lamp at the saved wake direction, then let the servos go quiet.
    currentPan = wakePan;
    currentTilt = wakeTilt;
    attachServos();
    detachServos();

    Serial.println(F("Keys: 0-9 time, A off, B new alarm, C aim, D test, * arm/disarm"));
    printStatus();

    if (alarmArmed)
        goIdle();
    else
        startEntry();
}

void loop()
{
    handleSerial();

    if (state == State::FAULT)
    {
        setLamp(0);
        return;
    }

    const char key = keypad.getKey();
    if (key)
        handleKey(key);

    checkAlarm();
    updateSequence();

    if (state == State::IDLE && millis() - lastScreenAt >= 1000)
    {
        lastScreenAt = millis();
        showIdleScreen();
    }
}
