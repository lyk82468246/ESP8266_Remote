#pragma once
#include "Config.h"
#include "Tlc549.h"
#include <core_esp8266_waveform.h>
#include <cmath>

class Peripheral {
 public:
  void begin(const config::ResourceConfig& spec) {
    if (spec_ && timedSensor()) detachInterrupt(digitalPinToInterrupt(interruptPin_));
    spec_ = &spec; value_ = 0; waveform_ = false;
    available_ = false; durationAvailable_ = false; measuring_ = false; durationMs_ = 0; activeSince_ = 0;
    interruptPin_ = spec.gpio; interruptActiveLow_ = spec.activeLow;
    edgeHead_ = edgeTail_ = 0; edgeOverflow_ = false;
    hasAcceptedEdge_ = false; lastAcceptedEdgeMs_ = 0;
    if (spec.type == config::ResourceType::Tlc549) tlc549::begin();
    if (digitalSensor()) {
      pinMode(spec.gpio, INPUT_PULLUP);
      available_ = true;
      value_ = (digitalRead(spec.gpio) == (spec.activeLow ? LOW : HIGH)) ? 1 : 0;
      if (timedSensor())
        attachInterruptArg(digitalPinToInterrupt(spec.gpio), &Peripheral::edgeISR, this, CHANGE);
      return;
    }
    available_ = writable();
    if (!writable()) return;
    digitalWrite(spec.gpio, spec.activeLow ? HIGH : LOW);
    pinMode(spec.gpio, OUTPUT);
  }
  const config::ResourceConfig& spec() const { return *spec_; }
  bool writable() const { return spec_->type == config::ResourceType::Binary || spec_->type == config::ResourceType::Pwm; }
  bool boolean() const { return spec_->type == config::ResourceType::Binary || digitalSensor(); }
  bool timedSensor() const { return spec_->type == config::ResourceType::TimedBinarySensor; }
  bool hasDuration() const { return timedSensor(); }
  bool durationAvailable() const { return hasDuration() && durationAvailable_; }
  uint32_t durationMs() const { return durationMs_; }
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

  bool processInterruptEvents() {
    if (!timedSensor()) return false;
    bool changed = false;
    EdgeEvent event;
    while (popEdge(event)) changed = acceptEdge(event.atMs, event.active) || changed;

    bool overflowed = false;
    noInterrupts();
    overflowed = edgeOverflow_;
    edgeOverflow_ = false;
    interrupts();
    if (overflowed) {
      // A burst larger than the small ISR queue is exceptional for a relay.
      // Resynchronize from the pin in the main context rather than running
      // GPIO, timing, or state-management code from the ISR.
      changed = acceptEdge(millis(),
                           digitalRead(interruptPin_) ==
                               (interruptActiveLow_ ? LOW : HIGH)) || changed;
    }
    return changed;
  }

  bool sample(uint32_t /*now*/) {
    if (writable()) return false;
    if (timedSensor()) return false;
    if (digitalSensor()) {
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
  struct EdgeEvent {
    uint32_t atMs;
    bool active;
  };
  static constexpr uint8_t kEdgeQueueSize = 4;

  static void IRAM_ATTR edgeISR(void* context) {
    Peripheral* self = static_cast<Peripheral*>(context);
    const uint8_t next = static_cast<uint8_t>((self->edgeHead_ + 1) % kEdgeQueueSize);
    if (next == self->edgeTail_) {
      self->edgeOverflow_ = true;
      return;
    }
    self->edgeTimesMs_[self->edgeHead_] = millis();
    self->edgeLevels_[self->edgeHead_] =
        digitalRead(self->interruptPin_) == (self->interruptActiveLow_ ? LOW : HIGH);
    self->edgeHead_ = next;
  }

  bool popEdge(EdgeEvent& event) {
    noInterrupts();
    if (edgeTail_ == edgeHead_) {
      interrupts();
      return false;
    }
    const uint8_t index = edgeTail_;
    event.atMs = edgeTimesMs_[index];
    event.active = edgeLevels_[index];
    edgeTail_ = static_cast<uint8_t>((index + 1) % kEdgeQueueSize);
    interrupts();
    return true;
  }

  bool acceptEdge(uint32_t atMs, bool active) {
    if (hasAcceptedEdge_ && static_cast<uint32_t>(atMs - lastAcceptedEdgeMs_) < config::kMicrowaveDebounceMs)
      return false;
    if (active == (value_ != 0)) return false;
    hasAcceptedEdge_ = true;
    lastAcceptedEdgeMs_ = atMs;
    if (active) {
      measuring_ = true;
      activeSince_ = atMs;
    } else if (measuring_) {
      uint32_t elapsed = atMs - activeSince_;
      if (elapsed > static_cast<uint32_t>(config::kMicrowaveDurationMaximumMs))
        elapsed = static_cast<uint32_t>(config::kMicrowaveDurationMaximumMs);
      durationMs_ = elapsed;
      durationAvailable_ = true;
      measuring_ = false;
    }
    value_ = active ? 1 : 0;
    available_ = true;
    return true;
  }

  bool digitalSensor() const {
    return spec_->type == config::ResourceType::BinarySensor || timedSensor();
  }
  bool externalAdc() const { return spec_->type == config::ResourceType::Tlc549; }
  const config::ResourceConfig* spec_ = nullptr;
  int32_t value_ = 0;
  bool available_ = false, waveform_ = false;
  bool durationAvailable_ = false, measuring_ = false;
  uint32_t durationMs_ = 0, activeSince_ = 0;
  uint8_t interruptPin_ = 255;
  bool interruptActiveLow_ = false;
  volatile uint8_t edgeHead_ = 0, edgeTail_ = 0;
  volatile bool edgeOverflow_ = false;
  volatile uint32_t edgeTimesMs_[kEdgeQueueSize] = {};
  volatile bool edgeLevels_[kEdgeQueueSize] = {};
  bool hasAcceptedEdge_ = false;
  uint32_t lastAcceptedEdgeMs_ = 0;
};
