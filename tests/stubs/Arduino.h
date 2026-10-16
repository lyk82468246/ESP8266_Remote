#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <sstream>
#include <string>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define A0 17
extern int pinLevels[17];
extern unsigned gpioWrites;
extern bool failReadback;
inline void digitalWrite(uint8_t gpio, int level) { pinLevels[gpio] = level; ++gpioWrites; }
inline void pinMode(uint8_t, int) {}
inline int digitalRead(uint8_t gpio) { return failReadback ? !pinLevels[gpio] : pinLevels[gpio]; }
extern int testAnalogValue;
extern unsigned analogReads;
inline int analogRead(uint8_t) { ++analogReads; return testAnalogValue; }
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }
class FakeSerial {
 public:
  std::string output;
  template <typename T> void print(const T& value) { std::ostringstream stream; stream << value; output += stream.str(); }
  template <typename T> void println(const T& value) { print(value); output += '\n'; }
  void printf(const char* format, ...) {
    char buffer[512]; va_list args; va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); va_end(args); output += buffer;
  }
};
extern FakeSerial Serial;
