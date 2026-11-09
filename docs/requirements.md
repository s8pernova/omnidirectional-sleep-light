# System Requirements

## Product requirements

| Requirement                                                                                             | Verification                                 |
| ------------------------------------------------------------------------------------------------------- | -------------------------------------------- |
| The device shall wake the intended user with directional light instead of a conventional audible alarm. | Consented user testing.                      |
| The device shall wake the intended user in at least 95% of validation trials.                           | At least 19 successful trials out of 20.     |
| The roommate shall remain undisturbed in at least 90% of validation trials.                             | At least 18 successful trials out of 20.     |
| Device-generated sound shall not exceed 30 dBA at 1 m.                                                  | Sound-meter test during complete activation. |
| The prototype shall cost no more than $75.                                                              | Final BOM and receipts.                      |
| The deployed assembly shall fit within 24 × 24 × 24 in and should weigh no more than 1 kg.              | Measurement of the completed assembly.       |
| A new user shall be able to configure and arm the alarm in no more than three steps and five minutes.   | Timed usability test.                        |
| The assembly shall remain functional for at least one semester.                                         | Inspection and repeated-cycle testing.       |

## Functional requirements

| Requirement                                                                                                                      |
| -------------------------------------------------------------------------------------------------------------------------------- |
| The device shall maintain wall-clock time with a battery-backed RTC.                                                             |
| The user shall be able to set an alarm without recompiling the firmware.                                                         |
| The alarm time and calibrated lamp direction shall survive an Uno reset.                                                         |
| At the alarm time, the device shall move the lamp to its stored pan and tilt angles.                                             |
| The device shall gradually increase lamp brightness over a configurable interval.                                                |
| A physical switch shall arm and disarm the alarm. Disarming shall turn off the lamp and stop automatic motion within one second. |
| Servo commands shall remain inside calibrated mechanical limits.                                                                 |
| A clock, configuration, or command fault shall leave the lamp off and motion inhibited.                                          |
| The firmware shall expose diagnostics and manual hardware tests over 9600-baud serial.                                           |

## Hardware requirements

| Requirement                                                                                                                            |
| -------------------------------------------------------------------------------------------------------------------------------------- |
| The controller shall be compatible with the Arduino Uno R3 target configured in PlatformIO.                                            |
| Two positional servos shall provide independent pan and tilt control. Continuous-rotation servos shall not be used.                    |
| The selected tilt servo shall have rated stall torque at least three times the maximum static load moment of the moving lamp assembly. |
| The servos shall use a regulated external supply sized for both motors' simultaneous peak current.                                     |
| The lamp shall use an external supply and a logic-level switching device rated for the lamp's voltage and current.                     |
| The Uno, servos, sensors, RTC, and lamp-control circuit shall share a common low-voltage ground.                                       |
| The support, joints, and wiring shall tolerate full permitted motion without slipping, collision, pinching, or wire strain.            |
| The device shall use an enclosed, listed low-voltage wall adapter and contain no exposed mains wiring.                                 |
| The completed device shall comply with applicable Virginia Tech housing and fire-safety rules before unattended dorm use.              |

## Prototype acceptance

The prototype is ready for integrated testing when all of the following pass:

- `pio run` builds successfully.
- The RTC retains valid time after main power is removed and restored.
- An alarm triggers once at the configured time.
- Both axes move through their calibrated ranges without collision, binding, resets, or wire strain.
- Simultaneous servo motion and maximum lamp output do not reset the Uno or move a supply outside its rated range.
- Lamp brightness increases monotonically through 0%, 25%, 50%, 75%, and 100% commands.
- The disarm switch safely interrupts every active state.
- One hundred pan/tilt/return cycles cause no loose fasteners, damage, or loss of calibration.

## Decisions required before physical assembly

> [!IMPORTANT]
> The team documents do not currently identify the following details.

1. Exact servo manufacturer, model, voltage range, peak/stall current, travel, and torque.
2. Completed lamp-and-linkage mass, center of mass, and required tilt torque.
3. Exact 3 W lamp model, input voltage/current, polarity, and dimming compatibility.
4. Final power adapter, servo regulator, protection, connectors, and logic-level lamp switch.
5. Final alarm-setting interface for users who will not have a development computer.
6. Whether webcam tracking is in scope and, if so, which host computer will process it.
7. Final pan/tilt limits and the required target-to-roommate light-level ratio.
