#pragma once
#include <Arduino.h>

// Override network settings in ignored config.local.h; never commit real credentials.
#if __has_include("config.local.h")
#include "config.local.h"
#endif
#ifndef REMOTE_USE_STA
#define REMOTE_USE_STA 0
#endif
#ifndef REMOTE_AP_PASSWORD
#define REMOTE_AP_PASSWORD "esp8266-demo"
#endif
#ifndef REMOTE_STA_SSID
#define REMOTE_STA_SSID ""
#endif
#ifndef REMOTE_STA_PASSWORD
#define REMOTE_STA_PASSWORD ""
#endif

namespace config {
constexpr uint16_t kPort = 80;
constexpr size_t kMaxResources = 8;
constexpr size_t kMaxBody = 1024;
// Eight binary outputs fit comfortably in 4 KiB; protocol permits up to 16 KiB.
constexpr size_t kMaxResponse = 4096;
constexpr uint32_t kIdleTimeoutMs = 1500;
constexpr uint32_t kRequestTimeoutMs = 5000;
constexpr const char* kFirmwareVersion = "0.1.0";
constexpr const char* kDeviceName = "ESP8266 外设控制器";

struct BinaryOutputConfig {
  const char* id;
  const char* name;
  uint8_t gpio;
  bool activeLow;
};
// GPIO numbers, not NodeMCU Dx labels. Default: NodeMCU LED, D4 = GPIO2.
constexpr BinaryOutputConfig kOutputs[] = {
    {"led.main", "板载 LED", 2, true},
    // Add only physically connected outputs, e.g. {"relay.1", "继电器", 5, true}.
};
constexpr size_t kOutputCount = sizeof(kOutputs) / sizeof(kOutputs[0]);
static_assert(kOutputCount > 0 && kOutputCount <= kMaxResources, "Invalid resource count");
}  // namespace config
