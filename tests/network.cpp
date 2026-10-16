#if defined(REMOTE_TEST_EMPTY_SSID)
#define REMOTE_STA_SSID ""
#else
#define REMOTE_STA_SSID "test-hotspot"
#endif
#define REMOTE_STA_PASSWORD "test-secret-never-log"
#include <cstdlib>
#include <iostream>
#include "NetworkService.h"

uint32_t testMillis = 0;
FakeSerial Serial;
FakeWiFi WiFi;
FakeMDNS MDNS;
unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
  std::cerr << "FAIL line " << __LINE__ << ": " << #condition << '\n'; std::exit(1); } } while (0)

int main() {
  NetworkService network;
#if defined(REMOTE_TEST_EMPTY_SSID)
  CHECK(!network.begin("esp8266-123456"));
  CHECK(WiFi.currentMode == WIFI_OFF);
  network.update(); CHECK(MDNS.begins == 0);
  CHECK(Serial.output.find("SSID is empty") != std::string::npos);
#else
  CHECK(network.begin("esp8266-123456"));
  CHECK(!WiFi.persist);
#if REMOTE_USE_STA
  CHECK(WiFi.currentMode == WIFI_STA);
  CHECK(WiFi.autoReconnect);
  CHECK(WiFi.ssid == "test-hotspot");
  network.update(); CHECK(MDNS.begins == 0);
  WiFi.connected = true; WiFi.address = 0x0201a8c0;
#else
  CHECK(WiFi.currentMode == WIFI_AP);
  CHECK(WiFi.ssid == "ESP8266-Remote-123456");
#endif
  network.update(); CHECK(MDNS.begins == 1);
  CHECK(MDNS.hostname == "esp8266-123456");
  CHECK(MDNS.instance == "WiFi-Control-esp8266-123456");
  CHECK(MDNS.service == "wifi-control" && MDNS.transport == "tcp" && MDNS.port == 80);
  CHECK(MDNS.txt["protocolVersion"] == "1" && MDNS.txt["deviceId"] == "esp8266-123456");
  CHECK(MDNS.updates == 1);
  network.update(); CHECK(MDNS.begins == 1 && MDNS.updates == 2);
#if REMOTE_USE_STA
  WiFi.connected = false; network.update(); CHECK(MDNS.ends == 1);
  network.update(); CHECK(MDNS.begins == 1);
  WiFi.connected = true; network.update(); CHECK(MDNS.begins == 2); // same IP reconnect
  WiFi.address = 0x0301a8c0; network.update(); CHECK(MDNS.begins == 3 && MDNS.ends == 2);
#else
  WiFi.apAddress = 0x0204a8c0; network.update(); CHECK(MDNS.begins == 2 && MDNS.ends == 1);
#endif
  // Failed begin, service registration and TXT registration remain retryable.
  for (unsigned failure = 0; failure < 3; ++failure) {
    MDNS = FakeMDNS();
    MDNS.allowBegin = failure != 0; MDNS.allowService = failure != 1; MDNS.allowTxt = failure != 2;
    NetworkService retrying; CHECK(retrying.begin("esp8266-123456"));
    retrying.update(); CHECK(MDNS.begins == 1 && MDNS.ends == 1 && MDNS.updates == 0);
    testMillis += config::kMdnsRetryMs - 1; retrying.update(); CHECK(MDNS.begins == 1);
    MDNS.allowBegin = MDNS.allowService = MDNS.allowTxt = true;
    ++testMillis; retrying.update(); CHECK(MDNS.begins == 2 && MDNS.updates == 1);
  }
  // Unsigned time arithmetic also works across millis() wraparound.
  MDNS = FakeMDNS(); MDNS.allowBegin = false;
  testMillis = 0xfffffff0; NetworkService wrapping; CHECK(wrapping.begin("esp8266-123456"));
  wrapping.update(); testMillis += config::kMdnsRetryMs;
  MDNS.allowBegin = true; wrapping.update(); CHECK(MDNS.begins == 2);
#endif
  CHECK(Serial.output.find("test-secret-never-log") == std::string::npos);
  std::cout << "PASS: " << checks << " network lifecycle assertions (STA=" << REMOTE_USE_STA << ")\n";
}
