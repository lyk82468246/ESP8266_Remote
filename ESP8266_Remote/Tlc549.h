#pragma once
#include "Config.h"

namespace tlc549 {
inline void begin() {
  pinMode(config::kTlc549Data, INPUT);
  digitalWrite(config::kTlc549Clock, LOW);
  pinMode(config::kTlc549Clock, OUTPUT);
  digitalWrite(config::kTlc549Cs, HIGH);
  pinMode(config::kTlc549Cs, OUTPUT);
}

inline uint8_t transfer() {
  digitalWrite(config::kTlc549Cs, LOW);
  delayMicroseconds(5);  // MSB appears when CS goes low.
  uint8_t code = 0;
  for (unsigned i = 0; i < 8; ++i) {
    code = (code << 1) | (digitalRead(config::kTlc549Data) == HIGH);
    digitalWrite(config::kTlc549Clock, HIGH);
    delayMicroseconds(25);
    digitalWrite(config::kTlc549Clock, LOW);
    delayMicroseconds(25);
  }
  digitalWrite(config::kTlc549Cs, HIGH);
  return code;
}

inline uint8_t read() {
  // First frame starts a fresh conversion on its eighth falling edge.
  // The second frame shifts that result out, not a potentially stale byte.
  transfer();
  delayMicroseconds(25);
  return transfer();
}
}  // namespace tlc549
