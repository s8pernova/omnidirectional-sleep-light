
#include <Wire.h>
#include <RTClib.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

// =====================================
// SETTINGS
// =====================================

// Set true for one upload to synchronize
// RTC using computer compilation time.
// Change to false after synchronization.
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

// Date prevents the same alarm from
// triggering repeatedly on the same day.
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

// Format time as HH:MM:SS
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

// Show current RTC and stored alarm
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
// RESET / NEW ALARM (B)
// =====================================

void resetAlarm() {
  state = ENTERING_TIME;

  arrindx = 0;

  for (int i = 0; i < 4; i++) {
    arr[i] = 0;
  }

  // Reset trigger history for new alarm
  lastTriggeredDate = 0;

  showEntryScreen();

  Serial.println("Enter a new alarm time.");
}

// =====================================
// sets alarm
// =====================================

void setAlarm() {
  alarmHour = arr[0] * 10 + arr[1];
  alarmMinute = arr[2] * 10 + arr[3];

  state = ALARM_SET;
  lastTriggeredDate = 0;

  showClockScreen();

  Serial.print("Alarm set for: ");
  Serial.println(formatAlarm());
}

// =====================================
// alarm triggers
// =====================================

void triggerAlarm() { //Big Function
  state = ALARM_ACTIVE;

  // Light Control Stuff
  // digitalWrite(lightPin, HIGH);

  showMessage(
    "ALARM ACTIVE!",
    "Press A to Stop"
  );

  Serial.println("ALARM TRIGGERED!");
}

// =====================================
// stops alarm
// =====================================

void stopAlarm() {
  if (state != ALARM_ACTIVE) {
    return;
  }

  state = ALARM_STOPPED;

  // FUTURE LIGHT CONTROL:
  // digitalWrite(lightPin, LOW);

  showClockScreen();

  Serial.println("Alarm stopped.");
}

// =====================================
// RTC Stuff
// =====================================

// Example:
// T2026-11-17 18:30:00

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

  if (state == ALARM_SET ||
      state == ALARM_STOPPED) {
    showClockScreen();
  }
}

// =====================================
// SETUP
// =====================================

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(100);
  Wire.begin();

  lcd.init();
  lcd.backlight();

  showMessage("Initializing...", "");

  if (!rtc.begin()) {
    showMessage("RTC Error!", "Check Wiring");
    Serial.println("RTC not found!");

    while (1);
  }

  if (SYNC_RTC_ON_UPLOAD || rtc.lostPower()) {
    rtc.adjust(
      DateTime(F(__DATE__), F(__TIME__))
    );

    Serial.println("RTC initialized.");
  }

  resetAlarm();

  Serial.println("System ready.");
  Serial.println("B = New Alarm");
  Serial.println("A = Stop Active Alarm");
  Serial.println("RTC command: TYYYY-MM-DD HH:MM:SS");
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {
  checkSerialTime();

  DateTime now = rtc.now();

  char key = keypad.getKey();

  // ===================================
  // Keypad stuff
  // ===================================

  if (key) {
    Serial.print("Pressed: ");
    Serial.println(key);

    // B resets
    if (key == 'B') {
      resetAlarm();
    }

    // A stops active alarm
    else if (key == 'A') {
      stopAlarm();
    }

    // Enter digits
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

          Serial.println("Invalid alarm time.");

          // Keep the invalid entry visible
          // briefly, then reset.
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

    // Calendar date as YYYYMMDD
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
  // LCD CLOCK UPDATE
  // ===================================

  if (millis() - lastLCDUpdate >= 1000) {
    lastLCDUpdate = millis();

    // Only refresh the clock screen
    // when the alarm is not active.
    if (
      state == ALARM_SET ||
      state == ALARM_STOPPED
    ) {
      showClockScreen();
    }

    // Print RTC time to Serial Monitor
    Serial.print("RTC: ");
    Serial.println(formatTime(rtc.now()));
  }
}

