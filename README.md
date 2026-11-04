# Omnidirectional Sleep Light

> [!NOTE]
> This project is in (very) early development.

A quiet, directional wake light for people who share a room. At a scheduled time, the device aims a light toward one bed and gradually increases its brightness, helping wake the intended user without playing an audible alarm.

The design combines a bed-mounted arm, a conical lamp enclosure, and a two-axis servo mount.

## How it works

1. A DS1307 real time clock (RTC) keeps track of the time.
2. At the configured alarm time, an Arduino Uno moves the lamp to stored pan and tilt angles.
3. The Uno gradually increases the lamp brightness through a MOSFET power switch.
4. A photoresistor measures ambient or lamp output for diagnostics.
5. A physical switch arms or disarms the alarm.

The baseline design does not need face tracking. The user positions or calibrates the lamp once, and the two servos reproduce that direction when the alarm activates.

## Proposed Uno pinout

| Function                    |   Pin |
| --------------------------- | ----: |
| Pan servo signal            |    D9 |
| Tilt servo signal           |   D10 |
| Lamp PWM/MOSFET input       |    D5 |
| Arm/disarm switch           |    D2 |
| Photoresistor analog output |    A0 |
| DS1307 SDA                  |    A4 |
| DS1307 SCL                  |    A5 |
| USB serial                  | D0/D1 |

The switch connects between D2 and ground and uses `INPUT_PULLUP`. The lamp is never connected directly to D5; D5 controls the external lamp power stage.

The Servo library uses Timer1 on the Uno, disabling ordinary `analogWrite()` PWM on D9 and D10. Lamp PWM is therefore assigned to D5.

## Power and physical safety

The physical design must:

- power the servos from a regulated external supply sized for both motors moving together;
- use the lamp's specified external voltage and current;
- connect all low-voltage grounds together;
- use only an enclosed, listed adapter for mains power;
- clamp servo commands to safe mechanical angles; and
- secure every overhead joint with mechanical fasteners and strain-relieved wiring.

## Firmware direction

The intended firmware is a non-blocking state machine:

```text
DISARMED → ARMED_WAITING → POSITIONING → FADING → ACTIVE
                                       ↘ FAULT
```

Using `millis()` instead of long `delay()` calls keeps the switch, clock, and fault handling responsive while the lamp is moving or fading.

Serial diagnostics use 9600 baud. The current hardware-integration milestone accepts these commands in the Wokwi terminal or physical serial monitor:

| Command | Action |
|---|---|
| `t` | Run a test wake sequence: position both servos, then fade the lamp on. |
| `x` | Cancel the sequence and turn the lamp off. |
| `s` | Print the state, switch, angles, brightness, sensor reading, and RTC time. |
| `h` | Print command help. |

The arm/disarm switch remains authoritative: a test cannot start while disarmed, and disarming interrupts positioning or fading immediately.

## Development

This is a PlatformIO Arduino project. Run commands from the repository root, where `platformio.ini` is located.

```bash
make build                 # compile only
make upload                # compile and upload to a connected Uno
make monitor               # open serial monitor at 9600 baud
make deploy                # upload, then open the monitor
make deploy BAUD=115200    # override monitor baud
```

Equivalent PlatformIO commands are:

```bash
pio run
pio run --target upload
pio device monitor --baud 9600
```

If the Uno is attached to Windows while PlatformIO runs in WSL, attach its USB device to WSL before uploading.

## Wokwi simulation

The repository contains `wokwi.toml` and `diagram.json`. Build the firmware first, then start the Wokwi simulator from VS Code.

```bash
pio run
```

The completed virtual circuit is expected to include:

- Arduino Uno;
- two servos on D9 and D10;
- DS1307 on A4/A5;
- photoresistor output on A0;
- arm/disarm switch on D2; and
- a normal simulated LED on D5 as a stand-in for the real lamp and MOSFET.

## Optional face tracking

The NexiGo N60 is a USB computer webcam, though an Uno cannot receive its video or perform 1080p image processing.

If face tracking is added, a laptop or Raspberry Pi should process the camera image and send bounded pan/tilt targets to the Uno over USB serial.
