#include "vision_camera.h"

#include <Arduino.h>
#include <esp_camera.h>

namespace CameraPins
{
    // Official XIAO ESP32-S3 Sense camera connector pin map. These signals use
    // internal GPIOs and do not overlap the exposed D0-D10 pins in main.cpp.
    constexpr int POWER_DOWN = -1;
    constexpr int RESET = -1;
    constexpr int XCLK = 10;
    constexpr int SCCB_SDA = 40;
    constexpr int SCCB_SCL = 39;
    constexpr int DATA_7 = 48;
    constexpr int DATA_6 = 11;
    constexpr int DATA_5 = 12;
    constexpr int DATA_4 = 14;
    constexpr int DATA_3 = 16;
    constexpr int DATA_2 = 18;
    constexpr int DATA_1 = 17;
    constexpr int DATA_0 = 15;
    constexpr int VSYNC = 38;
    constexpr int HREF = 47;
    constexpr int PCLK = 13;
} // namespace CameraPins

namespace
{
    bool cameraReady = false;

    const char *pixelFormatName(pixformat_t format)
    {
        switch (format)
        {
        case PIXFORMAT_RGB565:
            return "RGB565";
        case PIXFORMAT_GRAYSCALE:
            return "GRAYSCALE";
        case PIXFORMAT_JPEG:
            return "JPEG";
        default:
            return "OTHER";
        }
    }
} // namespace

namespace VisionCamera
{
    bool begin()
    {
        if (cameraReady)
        {
            return true;
        }

        if (!psramFound())
        {
            Serial.println(F("[Camera] unavailable: PSRAM was not detected."));
            return false;
        }

        camera_config_t config{};

        // Keep the camera clock away from channels/timers used by the servos
        // and lamp in main.cpp.
        config.ledc_channel = LEDC_CHANNEL_4;
        config.ledc_timer = LEDC_TIMER_2;

        config.pin_d0 = CameraPins::DATA_0;
        config.pin_d1 = CameraPins::DATA_1;
        config.pin_d2 = CameraPins::DATA_2;
        config.pin_d3 = CameraPins::DATA_3;
        config.pin_d4 = CameraPins::DATA_4;
        config.pin_d5 = CameraPins::DATA_5;
        config.pin_d6 = CameraPins::DATA_6;
        config.pin_d7 = CameraPins::DATA_7;
        config.pin_xclk = CameraPins::XCLK;
        config.pin_pclk = CameraPins::PCLK;
        config.pin_vsync = CameraPins::VSYNC;
        config.pin_href = CameraPins::HREF;
        config.pin_sccb_sda = CameraPins::SCCB_SDA;
        config.pin_sccb_scl = CameraPins::SCCB_SCL;
        config.pin_pwdn = CameraPins::POWER_DOWN;
        config.pin_reset = CameraPins::RESET;

        config.xclk_freq_hz = 20000000;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_240X240;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

        const esp_err_t result = esp_camera_init(&config);
        if (result != ESP_OK)
        {
            Serial.printf(
                "[Camera] unavailable: initialization failed (0x%X).\n",
                static_cast<unsigned int>(result));
            return false;
        }

        cameraReady = true;

        const sensor_t *sensor = esp_camera_sensor_get();
        if (sensor != nullptr)
        {
            Serial.printf(
                "[Camera] ready: sensor PID=0x%04X, 240x240 RGB565.\n",
                static_cast<unsigned int>(sensor->id.PID));
        }
        else
        {
            Serial.println(F("[Camera] ready: 240x240 RGB565."));
        }

        return true;
    }

    bool isReady()
    {
        return cameraReady;
    }

    bool captureDiagnosticFrame()
    {
        if (!cameraReady)
        {
            Serial.println(F("[Camera] capture skipped: camera is unavailable."));
            return false;
        }

        camera_fb_t *frame = esp_camera_fb_get();
        if (frame == nullptr)
        {
            Serial.println(F("[Camera] capture failed: no frame was returned."));
            return false;
        }

        // Copy the metadata before returning the driver's reusable buffer.
        const size_t width = frame->width;
        const size_t height = frame->height;
        const size_t byteCount = frame->len;
        const pixformat_t format = frame->format;
        esp_camera_fb_return(frame);

        Serial.printf(
            "[Camera] captured %ux%u %s frame (%u bytes).\n",
            static_cast<unsigned int>(width),
            static_cast<unsigned int>(height),
            pixelFormatName(format),
            static_cast<unsigned int>(byteCount));
        return true;
    }
} // namespace VisionCamera
