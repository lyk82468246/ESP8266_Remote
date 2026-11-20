#pragma once
#include "Arduino.h"
#include <functional>
#include <memory>
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
struct WiFiEventStationModeGotIP { IPAddress ip, mask, gw; };
struct WiFiEventStationModeDisconnected {};
using WiFiEventHandler = std::shared_ptr<void>;
class FakeWiFi {
 public:
  int currentMode = -1;
  bool connected = false, autoReconnect = false, persist = true, allowMode = true, allowAp = true;
  uint32_t address = 0, apAddress = 0x0104a8c0;
  std::string host, ssid, password;
  std::function<void(const WiFiEventStationModeGotIP&)> gotIpCallback;
  std::function<void(const WiFiEventStationModeDisconnected&)> disconnectedCallback;
  void persistent(bool enabled) { persist = enabled; }
  bool mode(int value) { currentMode = value; return allowMode; }
  void hostname(const char* value) { host = value; }
  void setAutoReconnect(bool enabled) { autoReconnect = enabled; }
  void begin(const char* name, const char* secret) { ssid = name; password = secret; }
  bool softAP(const char* name, const char* secret) { ssid = name; password = secret; return allowAp; }
  WiFiEventHandler onStationModeGotIP(std::function<void(const WiFiEventStationModeGotIP&)> callback) {
    gotIpCallback = std::move(callback); return std::make_shared<int>(0);
  }
  WiFiEventHandler onStationModeDisconnected(std::function<void(const WiFiEventStationModeDisconnected&)> callback) {
    disconnectedCallback = std::move(callback); return std::make_shared<int>(0);
  }
  void emitGotIp(uint32_t value) {
    connected = true; address = value;
    if (gotIpCallback) { WiFiEventStationModeGotIP event; event.ip = IPAddress(value); gotIpCallback(event); }
  }
  void emitDisconnected() {
    connected = false;
    if (disconnectedCallback) { WiFiEventStationModeDisconnected event; disconnectedCallback(event); }
  }
  int status() const { return connected ? WL_CONNECTED : 0; }
  IPAddress localIP() const { return IPAddress(address); }
  IPAddress softAPIP() const { return IPAddress(apAddress); }
};
extern FakeWiFi WiFi;
