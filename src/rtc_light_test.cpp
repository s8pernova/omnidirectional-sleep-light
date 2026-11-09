#include <RTClib.h>
#include <Wire.h>
RTC_DS3231 rtc;
int led = 9;
int brightness = 0;
int fadeAmount = 0;
int counter = 0;
bool on = false;

void setup()
{
    Serial.begin(9600);

    // Wait for serial monitor to connect
    while (!Serial)
        ;

    // Initialize the RTC module
    if (!rtc.begin())
    {
        Serial.println("Error: Could not find RTC module. Check your wiring!");
        while (1)
            ;
    }

    // If the battery was removed or it's a brand new module, sync it with your computer's time

    if (rtc.lostPower())
    {
        Serial.println("RTC lost power, setting the time...");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)) + TimeSpan(0, 0, 0, 8));
    }
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)) + TimeSpan(0, 0, 0, 8));
}

void loop()
{

    if (counter % 100 == 0)
    {
        DateTime now = rtc.now();

        // Print current Date
        Serial.print(now.year(), DEC);
        Serial.print('/');
        Serial.print(now.month(), DEC);
        Serial.print('/');
        Serial.print(now.day(), DEC);
        Serial.print("   ");

        // Print current Time
        Serial.print(now.hour(), DEC);
        Serial.print(':');
        Serial.print(now.minute(), DEC);
        Serial.print(':');
        Serial.println(now.second(), DEC);
        if (now.second() == 30)
        {
            Serial.println("Alarm");
            fadeAmount = 10;
        }
        if (now.second() == 0)
        {
            fadeAmount = -10;
        }
        analogWrite(led, brightness);
        Serial.println(brightness);
        brightness = constrain(fadeAmount + brightness, 0, 255);
    }
    delay(1000); // Update once every second
    // if ()
}

void loop2()
{
    // put your main code here, to run repeatedly:
    analogWrite(led, brightness);
    brightness = brightness + fadeAmount;
    if (brightness <= 0 || brightness >= 255)
    {
        fadeAmount = -fadeAmount;
    }
    delay(10);
}
