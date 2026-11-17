# ADR 0001: Add XIAO ESP32-S3 Sense for face tracking

## Status

Proposed

## Date

2026-11-17

## Context

Face tracking must run on the sleep light **without a laptop, Raspberry Pi, or other dumb extension.** The current ESP32 DevKit must remain the main controller, but it can't process video from the USB webcam while also controlling the rest of the device.

## Decision

Keep the current ESP32 DevKit and add a Seeed Studio XIAO ESP32-S3 Sense as a vision coprocessor. Replace the USB webcam with the XIAO Sense camera module.

- The XIAO performs face detection using its camera and PSRAM
- The XIAO sends face position and tracking status to the ESP32 DevKit over 3.3 V UART
- The ESP32 DevKit remains responsible for the servos, lamp, RTC, alarm state, and safety limits

## Consequences

- The project requires separate firmware targets for the ESP32 DevKit and XIAO
- The boards require crossed UART transmit and receive connections plus a common ground
- Wokwi can simulate the boards and UART messages, but not the camera input
