#pragma once
#include "Arduino.h"
extern bool failWaveform, failWaveformStop;
extern unsigned waveformWrites;
extern uint32_t highMicros[17], lowMicros[17];
inline int startWaveform(uint8_t pin, uint32_t high, uint32_t low, uint32_t = 0,
                         int8_t = -1, uint32_t = 0, bool = false) {
  ++waveformWrites;
  if (failWaveform) return false;
  highMicros[pin] = high; lowMicros[pin] = low;
  return true;
}
inline int stopWaveform(uint8_t pin) {
  ++waveformWrites;
  if (failWaveformStop) return false;
  highMicros[pin] = lowMicros[pin] = 0;
  return true;
}
