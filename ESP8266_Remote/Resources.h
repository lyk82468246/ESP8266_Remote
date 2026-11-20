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
    unsigned analogCount = 0, tlcCount = 0;
    bool occupied[A0 + 1] = {};
    for (size_t i = 0; i < count; ++i) {
      const auto& spec = specs[i];
      if (!validIdentifier(spec.id) || !validDisplayName(spec.name) ||
          !validIdentifier(spec.propertyId) || !validDisplayName(spec.propertyName) || !spec.unit) return false;
      if (spec.type == config::ResourceType::Tlc549) {
        if (++tlcCount > 1 || spec.gpio != config::kTlc549Cs || spec.activeLow ||
            spec.minimum != 0 || spec.maximum != config::kTlc549CodeCount - 1 || spec.step != 1 ||
            !strcmp(spec.propertyId, config::kVoltagePropertyId) ||
            occupied[config::kTlc549Clock] || occupied[config::kTlc549Data]) return false;
        occupied[config::kTlc549Clock] = occupied[config::kTlc549Data] = true;
      } else if (spec.type == config::ResourceType::Analog) {
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
        } else if (spec.type == config::ResourceType::BinarySensor ||
                   spec.type == config::ResourceType::TimedBinarySensor) {
          if (spec.minimum != 0 || spec.maximum != 1 || spec.step != 1) return false;
        } else if (spec.type == config::ResourceType::Pwm) {
          if (spec.minimum != 0 || spec.maximum != 100 || spec.step <= 0 || spec.step > 100 || 100 % spec.step) return false;
        } else return false;
      }
      if (occupied[spec.gpio]) return false;
      occupied[spec.gpio] = true;
      for (size_t j = 0; j < i; ++j)
        if (strcmp(spec.id, specs[j].id) == 0) return false;
    }
    count_ = count; revision_ = 0; firstSample_ = true; lastSample_ = 0;
    microwaveKnown_ = false; manualLedOverride_ = false; lastMicrowaveState_ = false;
    for (size_t i = 0; i < count_; ++i) resources_[i].begin(specs[i]);
    return true;
  }
  Peripheral* find(const char* id) {
    for (size_t i = 0; i < count_; ++i)
      if (strcmp(id, resources_[i].spec().id) == 0) return &resources_[i];
    return nullptr;
  }
  const Peripheral* find(const char* id) const {
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
    if (!strcmp(resource.spec().id, config::kOnboardLedResourceId)) manualLedOverride_ = true;
    if (isColorBinary(resource) && before != resource.value()) syncColorPwm(resource);
    return true;
  }
  void sample(uint32_t now) {
    for (size_t i = 0; i < count_; ++i)
      if (resources_[i].processInterruptEvents()) ++revision_;
    // GPIO interrupt events are processed every loop. Only analog sampling
    // and the potentiometer-driven PWM refresh remain rate-limited.
    if (firstSample_ || now - lastSample_ >= config::kSampleIntervalMs) {
      firstSample_ = false; lastSample_ = now;
      for (size_t i = 0; i < count_; ++i) if (resources_[i].sample(now)) ++revision_;
      syncPotPwm();
    }
    syncMicrowaveLed();
  }

 private:
  bool isColorBinary(const Peripheral& resource) const {
    return resource.spec().type == config::ResourceType::Binary &&
           (!strcmp(resource.spec().id, config::kRedBinaryResourceId) ||
            !strcmp(resource.spec().id, config::kGreenBinaryResourceId));
  }
  Peripheral* colorPwm(const Peripheral& binary) {
    if (!isColorBinary(binary)) return nullptr;
    return find(!strcmp(binary.spec().id, config::kRedBinaryResourceId)
                    ? config::kRedPwmResourceId
                    : config::kGreenPwmResourceId);
  }
  int32_t potPwmLevel() const {
    const Peripheral* pot = find(config::kPotResourceId);
    if (!pot || !pot->available()) return -1;
    // Round to the nearest percentage and saturate diagnostic values above the
    // nominal 10-bit scale at 100%; raw/voltage reporting remains unchanged.
    const int64_t numerator = static_cast<int64_t>(pot->value()) * 100 +
                              config::kAdcFullScaleCount / 2;
    const int32_t level = static_cast<int32_t>(numerator / config::kAdcFullScaleCount);
    return level > 100 ? 100 : (level < 0 ? 0 : level);
  }
  void applyPwm(Peripheral& pwm, int32_t target) {
    if (target < 0 || pwm.value() == target) return;
    const int32_t before = pwm.value();
    if (pwm.set(target) && before != pwm.value()) ++revision_;
  }
  void syncColorPwm(Peripheral& binary) {
    Peripheral* pwm = colorPwm(binary);
    if (!pwm) return;
    if (!binary.value()) {
      const int32_t level = potPwmLevel();
      if (level >= 0) applyPwm(*pwm, level);
    }
  }
  void syncPotPwm() {
    const int32_t level = potPwmLevel();
    if (level < 0) return;
    for (const char* binaryId : {config::kRedBinaryResourceId, config::kGreenBinaryResourceId}) {
      Peripheral* binary = find(binaryId);
      if (binary && !binary->value()) {
        Peripheral* pwm = colorPwm(*binary);
        if (pwm) applyPwm(*pwm, level);
      }
    }
  }
  void syncMicrowaveLed() {
    Peripheral* microwave = find(config::kMicrowaveResourceId);
    Peripheral* led = find(config::kOnboardLedResourceId);
    if (!microwave || !led || !microwave->available()) return;
    const bool detected = microwave->value() != 0;
    if (microwaveKnown_ && detected == lastMicrowaveState_) return;
    microwaveKnown_ = true;
    lastMicrowaveState_ = detected;
    const bool wasManualOverride = manualLedOverride_;
    manualLedOverride_ = false;
    const int32_t target = detected ? 1 : 0;
    if (wasManualOverride || led->value() != target) {
      if (led->value() != target && led->set(target)) ++revision_;
    }
  }

  Peripheral resources_[config::kMaxResources];
  size_t count_ = 0;
  uint64_t revision_ = 0;
  uint32_t lastSample_ = 0;
  bool firstSample_ = true;
  bool microwaveKnown_ = false, manualLedOverride_ = false, lastMicrowaveState_ = false;
};
