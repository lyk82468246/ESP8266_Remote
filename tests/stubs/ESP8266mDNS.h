#pragma once
#include "Arduino.h"
#include <map>
class FakeMDNS {
 public:
  unsigned begins = 0, ends = 0, updates = 0;
  bool allowBegin = true, allowService = true, allowTxt = true;
  std::string hostname, instance, service, transport;
  uint16_t port = 0;
  std::map<std::string, std::string> txt;
  bool begin(const char* value) { ++begins; hostname = value; return allowBegin; }
  void end() { ++ends; txt.clear(); }
  const void* addService(const char* name, const char* type, const char* protocol, uint16_t number) {
    instance = name; service = type; transport = protocol; port = number;
    return allowService ? this : nullptr;
  }
  void* addServiceTxt(const void*, const char* key, const char* value) {
    if (!allowTxt) return nullptr;
    txt[key] = value; return this;
  }
  void update() { ++updates; }
};
extern FakeMDNS MDNS;
