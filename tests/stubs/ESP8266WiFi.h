#pragma once
#include "Arduino.h"
#define WIFI_OFF 0
#define WIFI_STA 1
#define WIFI_AP 2
#define WL_CONNECTED 3
class IPAddress {
 public:
  explicit IPAddress(uint32_t value = 0) : value_(value) {}
  operator uint32_t() const { return value_; }
 private:
  uint32_t value_;
};
class FakeWiFi {
 public:
  int currentMode = -1;
  bool connected = false, autoReconnect = false, persist = true, allowMode = true, allowAp = true;
  uint32_t address = 0, apAddress = 0x0104a8c0;
  std::string host, ssid, password;
  void persistent(bool enabled) { persist = enabled; }
  bool mode(int value) { currentMode = value; return allowMode; }
  void hostname(const char* value) { host = value; }
  void setAutoReconnect(bool enabled) { autoReconnect = enabled; }
  void begin(const char* name, const char* secret) { ssid = name; password = secret; }
  bool softAP(const char* name, const char* secret) { ssid = name; password = secret; return allowAp; }
  int status() const { return connected ? WL_CONNECTED : 0; }
  IPAddress localIP() const { return IPAddress(address); }
  IPAddress softAPIP() const { return IPAddress(apAddress); }
};
extern FakeWiFi WiFi;
