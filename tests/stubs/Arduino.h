#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
extern int pinLevels[17];
extern unsigned gpioWrites;
extern bool failReadback;
inline void digitalWrite(uint8_t gpio, int level) { pinLevels[gpio] = level; ++gpioWrites; }
inline void pinMode(uint8_t, int) {}
inline int digitalRead(uint8_t gpio) { return failReadback ? !pinLevels[gpio] : pinLevels[gpio]; }
