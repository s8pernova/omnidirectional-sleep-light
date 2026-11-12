# ADR 0001: Use XIAO ESP32-S3 Sense for face tracking

## Status

Proposed

## Date

2026-11-12

## Context

Face tracking must run on the sleep light without a laptop or Raspberry Pi. The current ESP32 DevKit cannot practically process video from the USB webcam while also controlling the rest of the device.

## Decision

Replace the ESP32 DevKit and USB webcam with a Seeed Studio XIAO ESP32-S3 Sense and its camera module.

- Do face detection directly on the ESP32-S3
- Use the board's PSRAM for camera frames and face detection

## Consequences

- The PlatformIO target and GPIO assignments must change
- Wokwi can simulate the controller and peripherals, but not the camera input
