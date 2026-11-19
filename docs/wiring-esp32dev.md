# Wiring: ESP32 DevKit breadboard prototype

Firmware: `src/sleep_light_esp32dev.cpp`, PlatformIO env `esp32dev`
(`uv run --locked pio run -e esp32dev -t upload`).

## Parts

| Part | Notes |
| --- | --- |
| ESP32 DevKit (ESP32-WROOM-32, can marked "ISM 2.4GHz") | Powered from USB |
| 4x4 membrane keypad | 8-pin ribbon |
| 16x2 LCD with I2C backpack (PCF8574) | 4 pins: GND, VCC, SDA, SCL |
| DS3231 RTC module (or DS1307; set `USE_DS1307 1`) | Shares I2C with the LCD |
| 2x SG90 servo | Brown = GND, Red = V+, Orange = signal |
| RFP30N06LE N-channel logic-level MOSFET | TO-220 |
| LED lamp + 9 V battery | Lamp switched on its negative side |
| 220 Ω resistor, 10 kΩ resistor | MOSFET gate |
| 470-1000 µF electrolytic capacitor (recommended) | Across the servo 5 V rail |

## Breadboard rails

| Rail | Connect to |
| --- | --- |
| 3V3 rail | ESP32 `3V3` |
| 5V rail | ESP32 `VIN` (5 V from USB) |
| GND rails | ESP32 `GND`. Join every GND rail together. |

The 9 V battery never goes on a rail. Its positive lead goes only to the lamp.

## Pinout

| Device pin | ESP32 / breadboard |
| --- | --- |
| **LCD** GND | GND |
| LCD VCC | 5V rail (see the I2C voltage note) |
| LCD SDA | GPIO 21 |
| LCD SCL | GPIO 22 |
| **RTC** GND | GND |
| RTC VCC | 3V3 rail |
| RTC SDA | GPIO 21 (same row as the LCD SDA) |
| RTC SCL | GPIO 22 (same row as the LCD SCL) |
| RTC SQW / 32K | not connected |
| **Pan servo** brown | GND |
| Pan servo red | 5V rail |
| Pan servo orange | GPIO 18 |
| **Tilt servo** brown | GND |
| Tilt servo red | 5V rail |
| Tilt servo orange | GPIO 19 |
| **Keypad** pin 1 (R1) | GPIO 32 |
| Keypad pin 2 (R2) | GPIO 33 |
| Keypad pin 3 (R3) | GPIO 25 |
| Keypad pin 4 (R4) | GPIO 26 |
| Keypad pin 5 (C1) | GPIO 27 |
| Keypad pin 6 (C2) | GPIO 14 |
| Keypad pin 7 (C3) | GPIO 13 |
| Keypad pin 8 (C4) | GPIO 4 |
| **MOSFET** Gate | 220 Ω to GPIO 23; 10 kΩ from Gate to GND |
| MOSFET Drain | Lamp negative (−) |
| MOSFET Source | GND rail |
| **Lamp** positive (+) | 9 V battery + |
| **9 V battery** − | GND rail (common ground with the ESP32) |

Keypad pin 1 is the leftmost ribbon pin with the keys facing you. If the keys
come out wrong, swap `KEY_ROWS` and `KEY_COLS` in the firmware.

RFP30N06LE, flat printed face toward you, legs down: **Gate, Drain, Source**
from left to right. The metal tab is the drain.

## Lamp circuit

```
9V(+) ──── Lamp(+)
           Lamp(−) ──── Drain
GPIO23 ─[220Ω]─┬──────── Gate          RFP30N06LE
               └─[10kΩ]─ GND
                         Source ──── GND rail ──── 9V(−) and ESP32 GND
```

The 10 kΩ resistor holds the lamp off while the ESP32 boots. The 220 Ω resistor
limits gate current spikes from the PWM.

## Power notes

- **Servos:** Two SG90s on the USB 5 V `VIN` pin work for light loads. The
  firmware moves one servo at a time and releases both when idle. Put the
  capacitor across the 5V rail and GND, with the negative stripe on GND. If the
  ESP32 resets or the LCD flickers when the servos move, power the servos from
  a separate 5-6 V supply such as 4x AA, and keep the grounds joined. Never
  power them from the 9 V battery.
- **I2C voltage:** The ESP32 runs on 3.3 V. The RTC is on 3.3 V for that
  reason. Most I2C LCD backpacks need 5 V to be readable, and their pull-up
  resistors then pull SDA/SCL toward 5 V. This usually works on a prototype but
  is outside the ESP32's rating. The proper fix is a BSS138 bidirectional level
  shifter between the ESP32 and the LCD. You can also try the LCD on 3V3 first
  and turn the contrast pot on the back.
- **DS3231 modules (ZS-042)** have a coin-cell charging circuit. If you use a
  non-rechargeable CR2032, keep the module on 3.3 V or remove the charging
  diode/resistor.
- **Lamp:** PWM dimming works with a plain LED or an LED module with a series
  resistor. A lamp with its own driver board may flicker or not dim smoothly.
  A 9 V battery is fine for testing but won't last long at full brightness.

## Keypad controls

| Key | Action |
| --- | --- |
| 0-9 | Type the alarm time as HHMM (24 h) |
| A | Lamp off / stop |
| B | Set a new alarm time |
| C | Aim mode: 4/6 pan, 2/8 tilt, 5 save, C or # leave |
| D | Test the full wake sequence now |
| * | Arm / disarm the alarm |

The alarm time, armed state, and aim direction are saved to flash and survive
a reset.

## Wake sequence

1. At the alarm time, the lamp turns toward the saved direction, moving 1° per 20 ms.
2. The servos release so they don't hum.
3. Brightness rises from 0 to 100 % over 20 s on an exponential curve,
   `(e^(4t/20) − 1) / (e^4 − 1)`: about 3 % at 5 s, 12 % at 10 s, 36 % at
   15 s, and 100 % at 20 s.
4. The lamp stays at 100 % until **A** is pressed.

Set the RTC from the serial monitor (9600 baud): `T2026-11-18 13:45:00`
