# Omnidirectional Sleep Light

> [!NOTE]
> This project is in (very) early development.

A quiet, directional wake light for people who share a room. At a scheduled time, the device aims a light toward one bed and gradually increases its brightness, helping wake the intended user without playing an audible alarm.

## How it Works

1. A DS1307 real time clock (RTC) keeps track of the time.
2. At the configured alarm time, an ESP32 moves the lamp to stored pan and tilt angles.
3. The ESP32 gradually increases the lamp brightness through a MOSFET power switch.
4. A photoresistor measures ambient or lamp output for diagnostics.
5. A physical switch arms or disarms the alarm.

### First Setup

- Clone this repository and open its root folder in VS Code.
- Install the **PlatformIO IDE** and (optional) **Wokwi Simulator** extensions
- Run `make build` from the terminal. Use `make help` for all the Makefile commands.
- Enjoy! :)

For the reproducible command-line workflow used in CI, install [uv](https://docs.astral.sh/uv/getting-started/installation/) (CI uses uv 0.10.12), then run:

```bash
uv sync --locked
uv run --locked pio run -e esp32dev
```
