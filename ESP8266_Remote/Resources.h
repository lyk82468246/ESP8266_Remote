#pragma once
#include "Config.h"
#include "Validation.h"
#include "Peripheral.h"
#include <string.h>

class ResourceRegistry {
 public:
  bool begin(const config::ResourceConfig* specs = config::kResources, size_t count = config::kResourceCount) {
    // Validate the entire configuration before touching any pins.
    if (!validDisplayName(config::kDeviceName) || count > config::kMaxResources || (count && !specs)) return false;
    unsigned analogCount = 0;
    for (size_t i = 0; i < count; ++i) {
      const auto& spec = specs[i];
      if (!validIdentifier(spec.id) || !validDisplayName(spec.name) ||
          !validIdentifier(spec.propertyId) || !validDisplayName(spec.propertyName) || !spec.unit) return false;
      if (spec.type == config::ResourceType::Analog) {
        if (spec.gpio != A0 || ++analogCount > 1 || spec.activeLow ||
            spec.minimum != 0 || spec.maximum != config::kAdcReportedMaximum || spec.step != 1 ||
            !strcmp(spec.propertyId, config::kVoltagePropertyId) ||
            !validIdentifier(config::kVoltagePropertyId) || !validDisplayName(config::kVoltagePropertyName) ||
            !std::isfinite(config::kAdcFullScaleVolts) || config::kAdcFullScaleVolts <= 0 ||
            config::kAdcFullScaleCount <= 0 || config::kAdcFullScaleCount > config::kAdcReportedMaximum) return false;
      } else {
        if (!((spec.gpio == 0 || spec.gpio == 2 || spec.gpio == 4 || spec.gpio == 5) ||
              (spec.gpio >= 12 && spec.gpio <= 16))) return false;
        if (spec.type == config::ResourceType::Binary) {
          if (spec.minimum != 0 || spec.maximum != 1 || spec.step != 1) return false;
        } else if (spec.type == config::ResourceType::Pwm) {
          if (spec.minimum != 0 || spec.maximum != 100 || spec.step <= 0 || spec.step > 100 || 100 % spec.step) return false;
        } else return false;
      }
      for (size_t j = 0; j < i; ++j) {
        if (spec.gpio == specs[j].gpio || strcmp(spec.id, specs[j].id) == 0) return false;
      }
    }
    count_ = count; revision_ = 0; firstSample_ = true; lastSample_ = 0;
    for (size_t i = 0; i < count_; ++i) resources_[i].begin(specs[i]);
    return true;
  }
  Peripheral* find(const char* id) {
    for (size_t i = 0; i < count_; ++i)
      if (strcmp(id, resources_[i].spec().id) == 0) return &resources_[i];
    return nullptr;
  }
  Peripheral& at(size_t i) { return resources_[i]; }
  size_t size() const { return count_; }
  uint64_t revision() const { return revision_; }
  bool write(Peripheral& resource, int32_t target) {
    const int32_t before = resource.value();
    if (!resource.set(target)) return false;
    if (before != resource.value()) ++revision_;
    return true;
  }
  void sample(uint32_t now) {
    if (!firstSample_ && now - lastSample_ < config::kSampleIntervalMs) return;
    firstSample_ = false; lastSample_ = now;
    for (size_t i = 0; i < count_; ++i) if (resources_[i].sample()) ++revision_;
  }

 private:
  Peripheral resources_[config::kMaxResources];
  size_t count_ = 0;
  uint64_t revision_ = 0;
  uint32_t lastSample_ = 0;
  bool firstSample_ = true;
};
