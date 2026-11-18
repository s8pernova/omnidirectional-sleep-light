
#include <Wire.h>
#include <RTClib.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// =====================================
// LED SETTINGS - ARDUINO UNO
// =====================================

// Orange wire: D10
// White wire: GND

constexpr uint8_t LED_PWM_PIN = 10;

// 30-second sunrise duration
constexpr unsigned long FADE_DURATION = 30000UL;

// Exponential curve strength
// Higher = slower beginning, faster ending
constexpr float FADE_CURVE = 4.0f;

// Update PWM approximately every 10 ms
constexpr unsigned long LED_UPDATE_MS = 10UL;

unsigned long fadeStartTime = 0;
unsigned long lastFadeUpdate = 0;

bool lightAtFullBrightness = false;

// =====================================
// SETTINGS
// =====================================

// Set true for one upload to synchronize
// RTC using compilation time.
// Change back to false afterward.
const bool SYNC_RTC_ON_UPLOAD = false;

// =====================================
// KEYPAD
// =====================================

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(
  makeKeymap(keys),
  rowPins,
  colPins,
  ROWS,
  COLS
);

// =====================================
// RTC AND LCD
// =====================================

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================
// ALARM STATE
// =====================================

enum AlarmState {
  ENTERING_TIME,
  ALARM_SET,
  ALARM_ACTIVE,
  ALARM_STOPPED
};

AlarmState state = ENTERING_TIME;

// Time entry
int arr[4] = {0, 0, 0, 0};
int arrindx = 0;

// Stored alarm time
int alarmHour = 0;
int alarmMinute = 0;

// Prevent repeated triggering on same date
uint32_t lastTriggeredDate = 0;

// LCD refresh timer
unsigned long lastLCDUpdate = 0;

// =====================================
// LCD FUNCTIONS
// =====================================

void showMessage(String line1, String line2) {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));

  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
}

// Format RTC time as HH:MM:SS
String formatTime(DateTime t) {
  char buffer[9];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02d:%02d:%02d",
    t.hour(),
    t.minute(),
    t.second()
  );

  return String(buffer);
}

// Format alarm as HH:MM
String formatAlarm() {
  char buffer[6];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02d:%02d",
    alarmHour,
    alarmMinute
  );

  return String(buffer);
}

// Show current RTC and alarm
void showClockScreen() {
  DateTime now = rtc.now();

  showMessage(
    "Now:   " + formatTime(now),
    "Alarm: " + formatAlarm() + " B:R"
  );
}

// =====================================
// TIME ENTRY
// =====================================

bool checkValidity() {
  int hour = arr[0] * 10 + arr[1];
  int minute = arr[2] * 10 + arr[3];

  return hour < 24 && minute < 60;
}

String getEnteredTime() {
  String result = "";

  for (int i = 0; i < 4; i++) {
    if (i < arrindx) {
      result += String(arr[i]);
    } else {
      result += "_";
    }

    if (i == 1) {
      result += ":";
    }
  }

  return result;
}

void showEntryScreen() {
  showMessage(
    "Enter Time HHMM",
    getEnteredTime() + " B:Reset"
  );
}

// =====================================
// LIGHT CONTROL FUNCTIONS
// =====================================

// Completely turn off the light
void turnOffLight() {
  analogWrite(LED_PWM_PIN, 0);
  digitalWrite(LED_PWM_PIN, LOW);

  lightAtFullBrightness = false;

  Serial.println("Light OFF");
}

// Begin sunrise effect at 0% brightness
void startLight() {
  fadeStartTime = millis();
  lastFadeUpdate = fadeStartTime;

  lightAtFullBrightness = false;

  // Start completely dark
  analogWrite(LED_PWM_PIN, 0);

  Serial.println("Sunrise alarm started!");
}

// Increase brightness exponentially
// from 0 to 255 over 30 seconds
void updateLight() {

  // Only operate when alarm is active
  if (state != ALARM_ACTIVE) {
    return;
  }

  // After sunrise finishes, remain at 100%
  if (lightAtFullBrightness) {
    return;
  }

  unsigned long currentTime = millis();

  unsigned long elapsed =
    currentTime - fadeStartTime;

  // Reached 30 seconds
  if (elapsed >= FADE_DURATION) {

    analogWrite(LED_PWM_PIN, 255);

    lightAtFullBrightness = true;

    Serial.println(
      "Sunrise complete! Light at 100%."
    );

    return;
  }

  // Limit PWM updates to every 10 ms
  if (currentTime - lastFadeUpdate < LED_UPDATE_MS) {
    return;
  }

  lastFadeUpdate = currentTime;

  // Progress ranges from 0.0 to 1.0
  float progress =
    (float)elapsed / (float)FADE_DURATION;

  // Normalize exponential curve
  // Starts at exactly 0 and ends at 1
  float exponentialBrightness =
    (exp(FADE_CURVE * progress) - 1.0f) /
    (exp(FADE_CURVE) - 1.0f);

  // Convert normalized brightness to PWM
  int brightness =
    (int)(255.0f * exponentialBrightness + 0.5f);

  // Ensure valid PWM range
  brightness = constrain(brightness, 0, 255);

  analogWrite(LED_PWM_PIN, brightness);
}

// =====================================
// RESET / NEW ALARM (B)
// =====================================

void resetAlarm() {

  // Immediately switch light off
  turnOffLight();

  state = ENTERING_TIME;

  arrindx = 0;

  for (int i = 0; i < 4; i++) {
    arr[i] = 0;
  }

  lastTriggeredDate = 0;

  showEntryScreen();

  Serial.println("Enter a new alarm time.");
}

// =====================================
// SET ALARM
// =====================================

void setAlarm() {
  alarmHour = arr[0] * 10 + arr[1];
  alarmMinute = arr[2] * 10 + arr[3];

  state = ALARM_SET;
  lastTriggeredDate = 0;

  turnOffLight();

  showClockScreen();

  Serial.print("Alarm set for: ");
  Serial.println(formatAlarm());
}

// =====================================
// ALARM TRIGGERS
// =====================================

void triggerAlarm() {

  state = ALARM_ACTIVE;

  // Begin 30-second sunrise
  startLight();

  showMessage(
    "ALARM ACTIVE!",
    "B:Reset A:Stop"
  );

  Serial.println("ALARM TRIGGERED!");
}

// =====================================
// STOP ALARM (A)
// =====================================

void stopAlarm() {

  if (state != ALARM_ACTIVE) {
    return;
  }

  state = ALARM_STOPPED;

  // Immediately turn off light
  turnOffLight();

  showClockScreen();

  Serial.println("Alarm stopped.");
}

// =====================================
// RTC SERIAL TIME SYNCHRONIZATION
// =====================================

// Example:
// T2026-11-18 11:30:00

void checkSerialTime() {

  if (!Serial.available()) {
    return;
  }

  String input = Serial.readStringUntil('\n');
  input.trim();

  int year, month, day;
  int hour, minute, second;

  int parsed = sscanf(
    input.c_str(),
    "T%d-%d-%d %d:%d:%d",
    &year,
    &month,
    &day,
    &hour,
    &minute,
    &second
  );

  if (
    parsed != 6 ||
    input.length() != 20 ||
    input.charAt(0) != 'T' ||
    input.charAt(5) != '-' ||
    input.charAt(8) != '-' ||
    input.charAt(11) != ' ' ||
    input.charAt(14) != ':' ||
    input.charAt(17) != ':' ||
    year < 2000 || year > 2099 ||
    month < 1 || month > 12 ||
    day < 1 || day > 31 ||
    hour < 0 || hour > 23 ||
    minute < 0 || minute > 59 ||
    second < 0 || second > 59
  ) {
    Serial.println("Invalid format.");
    Serial.println("Use: TYYYY-MM-DD HH:MM:SS");
    return;
  }

  DateTime newTime(
    year, month, day,
    hour, minute, second
  );

  if (!newTime.isValid()) {
    Serial.println("Invalid calendar date.");
    return;
  }

  rtc.adjust(newTime);

  Serial.println("RTC synchronized!");

  if (
    state == ALARM_SET ||
    state == ALARM_STOPPED
  ) {
    showClockScreen();
  }
}

// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(9600);
  Serial.setTimeout(100);

  // Arduino Uno PWM output
  pinMode(LED_PWM_PIN, OUTPUT);

  // Ensure light starts completely off
  digitalWrite(LED_PWM_PIN, LOW);

  Serial.println("LED PWM initialized on D10");

  Wire.begin();

  lcd.init();
  lcd.backlight();

  showMessage("Initializing...", "");

  if (!rtc.begin()) {

    showMessage(
      "RTC Error!",
      "Check Wiring"
    );

    Serial.println("RTC not found!");

    while (1);
  }

  // Sync RTC if requested or if it lost power
  if (
    SYNC_RTC_ON_UPLOAD ||
    rtc.lostPower()
  ) {

    Serial.println("Synchronizing RTC...");

    const int UPLOAD_BUFFER_SECONDS = 8;

    DateTime computerTime =
      DateTime(F(__DATE__), F(__TIME__));

    rtc.adjust(
      computerTime +
      TimeSpan(0, 0, 0, UPLOAD_BUFFER_SECONDS)
    );

    Serial.println("RTC time restored!");
  }
  else {
    Serial.println("RTC running normally.");
  }

  Serial.println("RTC initialized.");

  resetAlarm();

  Serial.println("System ready.");

  Serial.println("B = Reset Alarm / Light OFF");
  Serial.println("A = Stop Alarm / Light OFF");

  Serial.println(
    "RTC command: TYYYY-MM-DD HH:MM:SS"
  );
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {

  checkSerialTime();

  DateTime now = rtc.now();

  char key = keypad.getKey();

  // ===================================
  // KEYPAD CONTROL
  // ===================================

  if (key) {

    Serial.print("Pressed: ");
    Serial.println(key);

    // B resets alarm and turns light off
    if (key == 'B') {
      resetAlarm();
    }

    // A stops active alarm
    else if (key == 'A') {
      stopAlarm();
    }

    // Enter alarm time
    else if (
      key >= '0' &&
      key <= '9' &&
      state == ENTERING_TIME &&
      arrindx < 4
    ) {

      arr[arrindx] = key - '0';

      arrindx++;

      showEntryScreen();

      // Four digits entered
      if (arrindx == 4) {

        if (checkValidity()) {
          setAlarm();
        }
        else {

          showMessage(
            "Invalid Time!",
            "B:Reset"
          );

          Serial.println(
            "Invalid alarm time."
          );

          delay(1500);

          resetAlarm();
        }
      }
    }
  }

  // ===================================
  // ALARM CHECK
  // ===================================

  if (state == ALARM_SET) {

    uint32_t today =
      (uint32_t)now.year() * 10000UL +
      (uint32_t)now.month() * 100UL +
      now.day();

    if (
      now.hour() == alarmHour &&
      now.minute() == alarmMinute &&
      today != lastTriggeredDate
    ) {

      lastTriggeredDate = today;

      triggerAlarm();
    }
  }

  // ===================================
  // SUNRISE LIGHT UPDATE
  // ===================================

  // Nonblocking exponential fade
  updateLight();

  // ===================================
  // LCD CLOCK UPDATE
  // ===================================

  if (millis() - lastLCDUpdate >= 1000) {

    lastLCDUpdate = millis();

    if (
      state == ALARM_SET ||
      state == ALARM_STOPPED
    ) {
      showClockScreen();
    }

    Serial.print("RTC: ");
    Serial.println(formatTime(rtc.now()));
  }
}
