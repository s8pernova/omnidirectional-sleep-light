#pragma once

namespace VisionCamera
{
    // Camera failures are non-fatal for now because Wokwi cannot emulate the
    // XIAO ESP32-S3 Sense camera. The rest of the sleep light remains usable.
    bool begin();
    bool isReady();
    bool captureDiagnosticFrame();
} // namespace VisionCamera
