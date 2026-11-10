# Omnidirectional Sleep Light

> [!NOTE]
> This project is in (very) early development.

A quiet, directional wake light for people who share a room. At a scheduled time, the device aims a light toward one bed and gradually increases its brightness, helping wake the intended user without playing an audible alarm.

## How it works

1. A DS1307 real time clock (RTC) keeps track of the time.
2. At the configured alarm time, an ESP32 moves the lamp to stored pan and tilt angles.
3. The ESP32 gradually increases the lamp brightness through a MOSFET power switch.
4. A photoresistor measures ambient or lamp output for diagnostics.
5. A physical switch arms or disarms the alarm.

The baseline design does not need face tracking. The user positions or calibrates the lamp once, and the two servos reproduce that direction when the alarm activates.

## ESP32 pinout

| Function                    | ESP32 pin                  |
| --------------------------- | -------------------------- |
| Pan servo signal            | GPIO18                     |
| Tilt servo signal           | GPIO19                     |
| Lamp PWM/MOSFET input       | GPIO27                     |
| Arm/disarm switch           | GPIO32                     |
| Photoresistor analog output | GPIO34                     |
| DS1307 SDA                  | GPIO21                     |
| DS1307 SCL                  | GPIO22                     |
| USB serial                  | Onboard USB-to-UART bridge |

## Firmware

The intended firmware is a state machine:

```text
DISARMED → ARMED_WAITING → POSITIONING → FADING → ACTIVE
                                       ↘ FAULT
```

Using `millis()` instead of long `delay()` calls keeps the switch, clock, and fault handling responsive while the lamp is moving or fading.

Serial diagnostics use 9600 baud. The current hardware-integration milestone accepts these commands in the Wokwi terminal or physical serial monitor:

| Command | Action                                                                     |
| ------- | -------------------------------------------------------------------------- |
| `t`     | Run a test wake sequence: position both servos, then fade the lamp on.     |
| `x`     | Cancel the sequence and turn the lamp off.                                 |
| `s`     | Print the state, switch, angles, brightness, sensor reading, and RTC time. |
| `h`     | Print command help.                                                        |

The arm/disarm switch remains authoritative: a test cannot start while disarmed, and disarming interrupts positioning or fading immediately.

### First-time setup

- Clone this repository and open its root folder in VS Code.
- Install the **PlatformIO IDE** and (optional) **Wokwi Simulator** extensions
- Run `make build` from the terminal. Use `make help` for all the Makefile commands.
- Enjoy! :)

For the reproducible command-line workflow used in CI, install [uv](https://docs.astral.sh/uv/getting-started/installation/) (CI uses uv 0.10.12), then run:

```bash
uv sync --locked
uv run --locked pio run -e esp32dev
```
