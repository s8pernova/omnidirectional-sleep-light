# ADR 0001: Replace the ESP32 DevKit with XIAO ESP32-S3 Sense

## Status

Accepted

## Date

2026-11-17

## Context

Face tracking must run on the sleep light **without a laptop, Raspberry Pi, or other dumb extension.** The current ESP32 DevKit cannot practically process video from the USB webcam while controlling the rest of the device.

## Decision

Replace the ESP32 DevKit and USB webcam with one Seeed Studio XIAO ESP32-S3 Sense and its camera module.

- Perform face detection directly on the XIAO using its camera and PSRAM
- Use the XIAO as the only controller for the servos, lamp, RTC, alarm state, and safety limits
- Keep peripheral wiring on exposed 3.3 V GPIOs that are not reserved by the camera

## Consequences

- The project has one controller and one firmware target
- The PlatformIO target, GPIO assignments, documentation, and Wokwi diagram must change
- Wokwi can simulate the XIAO and its external peripherals, but not the camera input
- Face tracking must be verified on physical hardware
