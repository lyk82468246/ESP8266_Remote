#pragma once
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <string.h>
#include "Config.h"

class NetworkService {
 public:
  bool begin(const char* deviceId) {
    snprintf(hostname_, sizeof(hostname_), "%s", deviceId);
    snprintf(instance_, sizeof(instance_), "WiFi-Control-%s", hostname_);
    WiFi.persistent(false);
#if REMOTE_USE_STA
    if (!REMOTE_STA_SSID[0]) {
      WiFi.mode(WIFI_OFF);
      Serial.println("STA SSID is empty. Configure WiFiConfig.h and rebuild. Outputs remain off.");
      return false;
    }
    if (!WiFi.mode(WIFI_STA)) return false;
    WiFi.hostname(hostname_);
    WiFi.setAutoReconnect(true);
    WiFi.begin(REMOTE_STA_SSID, REMOTE_STA_PASSWORD);
    Serial.println("STA connecting to configured hotspot/WLAN; waiting for DHCP address.");
#else
    if (!WiFi.mode(WIFI_AP)) return false;
    char ssid[32];
    const size_t length = strlen(hostname_);
    snprintf(ssid, sizeof(ssid), "ESP8266-Remote-%s", hostname_ + (length > 6 ? length - 6 : 0));
    if (!WiFi.softAP(ssid, REMOTE_AP_PASSWORD)) {
      Serial.println("SoftAP setup failed. API disabled."); return false;
    }
    Serial.printf("SSID: %s\n", ssid);
#endif
    started_ = true;
    return true;
  }

  // Run before HTTP early returns, including while idle or handling a slow connection.
  void update() {
    if (!started_) return;
#if REMOTE_USE_STA
    const uint32_t address = WiFi.status() == WL_CONNECTED ? static_cast<uint32_t>(WiFi.localIP()) : 0;
#else
    const uint32_t address = static_cast<uint32_t>(WiFi.softAPIP());
#endif
    if (address != address_) {
      if (mdnsActive_) MDNS.end();
      mdnsActive_ = false; address_ = address; retryNow_ = true;
      if (!address) Serial.println("Network disconnected; outputs retained.");
      else {
        Serial.print("API: http://"); Serial.print(IPAddress(address)); Serial.println("/api/v1/device");
      }
    }
    if (!address) return;
    const uint32_t now = millis();
    if (!mdnsActive_ && (retryNow_ || now - lastAttempt_ >= config::kMdnsRetryMs)) {
      retryNow_ = false; lastAttempt_ = now;
      if (!MDNS.begin(hostname_)) { discoveryFailed(); return; }
      const auto service = MDNS.addService(instance_, "wifi-control", "tcp", config::kPort);
      if (!service || !MDNS.addServiceTxt(service, "protocolVersion", "1") ||
          !MDNS.addServiceTxt(service, "deviceId", hostname_)) {
        discoveryFailed(); return;
      }
      mdnsActive_ = true;
      Serial.printf("mDNS: %s._wifi-control._tcp.local.\n", instance_);
    }
    if (mdnsActive_) MDNS.update();
  }

 private:
  char hostname_[24] = {}, instance_[40] = {};
  uint32_t address_ = 0, lastAttempt_ = 0;
  bool started_ = false, mdnsActive_ = false, retryNow_ = true;
  void discoveryFailed() {
    MDNS.end(); mdnsActive_ = false;
    Serial.println("mDNS unavailable; retry in 5 seconds. Manual IP remains available.");
  }
};
