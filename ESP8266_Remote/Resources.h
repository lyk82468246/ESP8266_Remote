#pragma once
#include "Config.h"
#include <string.h>

inline bool validIdentifier(const char* text) {
  if (!text) return false;
  const size_t length = strlen(text);
  if (length == 0 || length > 48) return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = text[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
  }
  return true;
}

class BinaryOutput {
 public:
  void begin(const config::BinaryOutputConfig& spec) {
    spec_ = &spec;
    // Set inactive latch before enabling output to avoid a startup pulse.
    digitalWrite(spec.gpio, spec.activeLow ? HIGH : LOW);
    pinMode(spec.gpio, OUTPUT);
    on_ = false;
  }
  bool set(bool on) {
    digitalWrite(spec_->gpio, (on != spec_->activeLow) ? HIGH : LOW);
    if ((digitalRead(spec_->gpio) == HIGH) != (on != spec_->activeLow)) {
      digitalWrite(spec_->gpio, (on_ != spec_->activeLow) ? HIGH : LOW);
      return false;
    }
    on_ = on;
    return true;
  }
  bool value() const { return on_; }
  const config::BinaryOutputConfig& spec() const { return *spec_; }

 private:
  const config::BinaryOutputConfig* spec_ = nullptr;
  bool on_ = false;
};

class ResourceRegistry {
 public:
  bool begin() {
    // Validate the entire configuration before touching any pins.
    for (size_t i = 0; i < config::kOutputCount; ++i) {
      const auto& spec = config::kOutputs[i];
      if (!validIdentifier(spec.id) || !spec.name ||
          !((spec.gpio == 0 || spec.gpio == 2 || spec.gpio == 4 || spec.gpio == 5) ||
            (spec.gpio >= 12 && spec.gpio <= 16))) return false;
      for (size_t j = 0; j < i; ++j) {
        if (spec.gpio == config::kOutputs[j].gpio ||
            strcmp(spec.id, config::kOutputs[j].id) == 0) return false;
      }
    }
    for (size_t i = 0; i < config::kOutputCount; ++i) outputs_[i].begin(config::kOutputs[i]);
    return true;
  }
  BinaryOutput* find(const char* id) {
    for (size_t i = 0; i < config::kOutputCount; ++i)
      if (strcmp(id, outputs_[i].spec().id) == 0) return &outputs_[i];
    return nullptr;
  }
  BinaryOutput& at(size_t i) { return outputs_[i]; }

 private:
  BinaryOutput outputs_[config::kOutputCount];
};
