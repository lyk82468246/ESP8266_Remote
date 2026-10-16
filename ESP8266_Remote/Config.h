#pragma once
#include <Arduino.h>
#include "WiFiConfig.h"

namespace config {
constexpr uint16_t kPort = 80;
constexpr size_t kMaxResources = 8;
constexpr size_t kMaxBody = 1024;
// Current six-resource description fits in 4 KiB; protocol permits up to 16 KiB.
constexpr size_t kMaxResponse = 4096;
constexpr uint32_t kIdleTimeoutMs = 1500;
constexpr uint32_t kRequestTimeoutMs = 5000;
constexpr uint32_t kMdnsRetryMs = 5000;
// The registry is immutable during a boot. A configuration flash creates a new bootId.
constexpr uint32_t kCapabilitiesRevision = 2;
constexpr const char* kFirmwareVersion = "0.4.0";
constexpr const char* kDeviceName = "ESP8266 外设控制器";
constexpr uint32_t kSampleIntervalMs = 250;
constexpr uint32_t kPwmPeriodUs = 1000;  // 1 kHz LED PWM.
// User-confirmed board-level 3.3 V scale; SDK system_adc_read uses 1/1024 V units
// at the chip. This is an estimate, not an overvoltage measurement or calibration.
constexpr int32_t kAdcFullScaleCount = 1024;
constexpr double kAdcFullScaleVolts = 3.3;
// Preserve the uint16 SDK result for diagnostics, even above the nominal scale.
constexpr int32_t kAdcReportedMaximum = 65535;
constexpr const char* kVoltagePropertyId = "voltage";
constexpr const char* kVoltagePropertyName = "电压（估算）";

enum class ResourceType { Binary, Pwm, Analog };
struct ResourceConfig {
  const char* id;
  const char* name;
  const char* propertyId;
  const char* propertyName;
  ResourceType type;
  uint8_t gpio;
  bool activeLow;
  int32_t minimum;
  int32_t maximum;
  int32_t step;
  const char* unit;
};
// NodeMCU V3: D1=5, D2=4, D5=14, D6=12. Every external LED needs its own resistor.
// GPIO -> 1 kohm -> LED anode; LED cathode -> GND. No motor connected to these pins.
constexpr ResourceConfig kResources[] = {
    {"led.main", "板载 LED", "on", "开关", ResourceType::Binary, 2, true, 0, 1, 1, ""},
    {"led.red", "红色开关灯", "on", "开关", ResourceType::Binary, 5, false, 0, 1, 1, ""},
    {"led.green", "绿色开关灯", "on", "开关", ResourceType::Binary, 4, false, 0, 1, 1, ""},
    {"pwm.red", "红色调光灯", "level", "亮度", ResourceType::Pwm, 14, false, 0, 100, 1, "%"},
    {"pwm.green", "绿色调光灯", "level", "亮度", ResourceType::Pwm, 12, false, 0, 100, 1, "%"},
    {"sensor.pot", "电位器", "raw", "ADC 原始值", ResourceType::Analog, A0, false, 0, kAdcReportedMaximum, 1, "ADC"},
};
constexpr size_t kResourceCount = sizeof(kResources) / sizeof(kResources[0]);
static_assert(kResourceCount <= kMaxResources, "Invalid resource count");
}  // namespace config
