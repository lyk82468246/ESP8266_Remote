#pragma once
#include "Config.h"
#include "Tlc549.h"
#include <core_esp8266_waveform.h>
#include <cmath>

class Peripheral {
 public:
  void begin(const config::ResourceConfig& spec) {
    spec_ = &spec; value_ = 0; waveform_ = false;
    available_ = false;
    if (spec.type == config::ResourceType::Tlc549) tlc549::begin();
    if (spec.type == config::ResourceType::BinarySensor) {
      pinMode(spec.gpio, INPUT_PULLUP);
      available_ = true;
      value_ = (digitalRead(spec.gpio) == (spec.activeLow ? LOW : HIGH)) ? 1 : 0;
      return;
    }
    available_ = writable();
    if (!writable()) return;
    digitalWrite(spec.gpio, spec.activeLow ? HIGH : LOW);
    pinMode(spec.gpio, OUTPUT);
  }
  const config::ResourceConfig& spec() const { return *spec_; }
  bool writable() const { return spec_->type == config::ResourceType::Binary || spec_->type == config::ResourceType::Pwm; }
  bool boolean() const { return spec_->type == config::ResourceType::Binary || spec_->type == config::ResourceType::BinarySensor; }
  bool hasVoltage() const { return spec_->type == config::ResourceType::Analog || externalAdc(); }
  int32_t value() const { return value_; }
  bool available() const { return available_; }
  bool voltageAvailable() const {
    return hasVoltage() && available_ && (externalAdc() || value_ <= config::kAdcFullScaleCount);
  }
  double voltageScale() const {
    return externalAdc() ? config::kTlc549ReferenceVolts : config::kAdcFullScaleVolts;
  }
  double voltage() const {
    const int32_t codes = externalAdc() ? config::kTlc549CodeCount : config::kAdcFullScaleCount;
    return std::round(value_ * voltageScale() / codes * 1000.0) / 1000.0;
  }

  bool set(int32_t target) {
    if (!writable() || target < spec_->minimum || target > spec_->maximum ||
        (target - spec_->minimum) % spec_->step != 0) return false;
    if (boolean()) {
      digitalWrite(spec_->gpio, (static_cast<bool>(target) != spec_->activeLow) ? HIGH : LOW);
      if ((digitalRead(spec_->gpio) == HIGH) != (static_cast<bool>(target) != spec_->activeLow)) {
        digitalWrite(spec_->gpio, (static_cast<bool>(value_) != spec_->activeLow) ? HIGH : LOW);
        return false;
      }
    } else {
      if (target == value_) return true;
      const uint32_t duty = spec_->activeLow ? 100 - target : target;
      if (duty == 0 || duty == 100) {
        if (waveform_ && !stopWaveform(spec_->gpio)) return false;
        waveform_ = false;
        digitalWrite(spec_->gpio, duty ? HIGH : LOW);
      } else {
        const uint32_t high = config::kPwmPeriodUs * duty / 100;
        // Use the Core waveform API to observe failure; analogWrite itself returns void.
        if (!startWaveform(spec_->gpio, high, config::kPwmPeriodUs - high)) return false;
        waveform_ = true;
      }
    }
    value_ = target;
    return true;
  }

  bool sample() {
    if (writable()) return false;
    if (spec_->type == config::ResourceType::BinarySensor) {
      const bool active = digitalRead(spec_->gpio) == (spec_->activeLow ? LOW : HIGH);
      const bool changed = !available_ || value_ != static_cast<int32_t>(active);
      available_ = true;
      if (active) value_ = 1; else value_ = 0;
      return changed;
    }
    const int raw = externalAdc() ? tlc549::read() : analogRead(spec_->gpio);
    const bool valid = raw >= spec_->minimum && raw <= spec_->maximum;
    const bool changed = valid != available_ || (valid && raw != value_);
    available_ = valid;
    if (valid) value_ = raw;
    return changed;
  }

 private:
  bool externalAdc() const { return spec_->type == config::ResourceType::Tlc549; }
  const config::ResourceConfig* spec_ = nullptr;
  int32_t value_ = 0;
  bool available_ = false, waveform_ = false;
};
