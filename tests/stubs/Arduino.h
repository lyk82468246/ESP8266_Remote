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
#define INPUT 0
#define INPUT_PULLUP 2
#define CHANGE 2
#define IRAM_ATTR
#define A0 17
extern int pinLevels[17];
extern unsigned gpioWrites;
extern bool failReadback;
extern void (*testGpioWriteHook)(uint8_t, int);
extern int (*testGpioReadHook)(uint8_t);
extern void (*testInterruptHandler)(void*);
extern void* testInterruptContext;
extern uint8_t testInterruptPin;
extern uint32_t testMicros;
inline void delayMicroseconds(uint32_t us) { testMicros += us; }
inline void digitalWrite(uint8_t gpio, int level) {
  pinLevels[gpio] = level; ++gpioWrites;
  if (testGpioWriteHook) testGpioWriteHook(gpio, level);
}
inline void pinMode(uint8_t, int) {}
inline int digitalRead(uint8_t gpio) {
  if (testGpioReadHook) return testGpioReadHook(gpio);
  return failReadback ? !pinLevels[gpio] : pinLevels[gpio];
}
inline int digitalPinToInterrupt(uint8_t pin) { return pin < 16 ? pin : -1; }
inline void attachInterruptArg(uint8_t pin, void (*handler)(void*), void* context, int) {
  testInterruptPin = pin; testInterruptHandler = handler; testInterruptContext = context;
}
inline void detachInterrupt(uint8_t pin) {
  if (testInterruptPin == pin) {
    testInterruptPin = 255; testInterruptHandler = nullptr; testInterruptContext = nullptr;
  }
}
inline void triggerInterrupt() {
  if (testInterruptHandler) testInterruptHandler(testInterruptContext);
}
inline void noInterrupts() {}
inline void interrupts() {}
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
