#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include "Api.h"

int pinLevels[17] = {};
unsigned gpioWrites = 0;
bool failReadback = false;
int testAnalogValue = 0;
unsigned analogReads = 0;
uint32_t testMillis = 0;
bool failWaveform = false, failWaveformStop = false;
unsigned waveformWrites = 0;
uint32_t highMicros[17] = {}, lowMicros[17] = {};
unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
  std::cerr << "FAIL line " << __LINE__ << ": " << #condition << '\n'; std::exit(1); } } while (0)

static HttpRequest parse(const std::string& wire) {
  HttpRequest request;
  for (const char c : wire) request.feed(c);
  return request;
}
static HttpRequest make(const std::string& method, const std::string& path,
                        const std::string& body = "", const std::string& media = "application/json") {
  return parse(method + " " + path + " HTTP/1.1\r\nHost: board\r\nContent-Type: " + media +
               "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body);
}
static const char* endpoint = "/api/v1/resources/led.main/properties/on";
static const char* uuid = "b294c92a-02e2-4b4f-b44e-254908bdcbf3";
static std::string payload(const std::string& boot, const std::string& value) {
  return "{\"requestId\":\"" + std::string(uuid) + "\",\"expectedBootId\":\"" + boot + "\",\"value\":" + value + "}";
}

int main(int argc, char** argv) {
  CHECK(!validDisplayName(nullptr)); CHECK(!validDisplayName(""));
  CHECK(!validDisplayName("   ")); CHECK(!validDisplayName("\tLED"));
  CHECK(!validDisplayName("LED\x7f")); CHECK(!validDisplayName("\xc2\x85"));
  CHECK(!validDisplayName("\xc2\xa0\xe3\x80\x80"));
  CHECK(validDisplayName("\xe6\x9d\xbf\xe8\xbd\xbd LED"));
  CHECK(validDisplayName("\xf0\x9f\x92\xa1"));
  CHECK(validDisplayName(std::string(64, 'x').c_str()));
  CHECK(!validDisplayName(std::string(65, 'x').c_str()));
  std::string unicodeBoundary;
  for (int i = 0; i < 64; ++i) unicodeBoundary += "\xf0\x9f\x92\xa1";
  CHECK(validDisplayName(unicodeBoundary.c_str()));
  unicodeBoundary += "x"; CHECK(!validDisplayName(unicodeBoundary.c_str()));
  for (const auto& invalid : {"\xc0\xaf", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe6\x9d", "\x80"})
    CHECK(!validDisplayName(invalid));
  ResourceRegistry registry;
  CHECK(registry.begin());
  CHECK(pinLevels[2] == HIGH);
  CHECK(!registry.at(0).value());
  Api api(registry); api.begin(0x123456, 1, 2);
  api.handle(make("GET", "/api/v1/device"));
  CHECK(api.status() == 200);
  CHECK(api.response()["protocolVersion"] == 1);
  CHECK(api.response()["capabilitiesRevision"].is<uint32_t>());
  CHECK(api.response()["capabilitiesRevision"] == config::kCapabilitiesRevision);
  const std::string deviceId = api.response()["deviceId"].as<const char*>();
  CHECK(deviceId == api.deviceId());
  std::string descriptionJson; serializeJson(api.response(), descriptionJson);
  CHECK(api.response()["resources"].size() == config::kResourceCount);
  CHECK(api.response()["resources"][0]["properties"][0]["type"] == "boolean");
  CHECK(api.response()["resources"][0]["properties"][0]["writable"] == true);
  CHECK(api.response()["resources"][5]["properties"].size() == 2);
  CHECK(api.response()["resources"][5]["properties"][1]["id"] == "voltage");
  CHECK(api.response()["resources"][5]["properties"][1]["type"] == "number");
  CHECK(api.response()["resources"][5]["properties"][1]["writable"] == false);
  CHECK(api.response()["resources"][5]["properties"][1]["unit"] == "V");
  const std::string boot = api.response()["bootId"].as<const char*>();
  api.handle(make("GET", "/api/v1/state"));
  CHECK(api.response()["revision"] == 0);
  CHECK(api.response()["capabilitiesRevision"] == config::kCapabilitiesRevision);
  CHECK(api.response()["deviceId"] == deviceId);
  CHECK(api.response()["bootId"] == boot);
  std::string stateJson; serializeJson(api.response(), stateJson);
  CHECK(api.response()["resources"][5]["values"]["voltage"].isNull());
  CHECK(api.response()["resources"][5]["quality"]["voltage"] == "unavailable");
  CHECK(api.response()["resources"][0]["values"]["on"] == false);
  CHECK(api.response()["resources"][0]["quality"]["on"] == "good");
  api.handle(make("PUT", endpoint, payload(boot, "true")));
  CHECK(api.status() == 200);
  CHECK(pinLevels[2] == LOW);
  CHECK(api.response()["value"] == true);
  CHECK(api.response()["requestId"] == uuid);
  CHECK(api.response()["revision"] == 1);
  std::string receiptJson; serializeJson(api.response(), receiptJson);
  api.handle(make("PUT", endpoint, payload(boot, "true")));
  CHECK(api.response()["revision"] == 1);
  CHECK(registry.at(0).value());
  api.handle(make("GET", "/api/v1/state"));
  CHECK(api.response()["capabilitiesRevision"] == config::kCapabilitiesRevision);
  CHECK(api.response()["revision"] == 1);
  auto reject = [&](const HttpRequest& request, int status, const char* code) {
    CHECK(request.complete && !request.errorStatus);
    const unsigned before = gpioWrites;
    api.handle(request);
    CHECK(api.status() == status);
    CHECK(api.response()["error"]["code"] == code);
    CHECK(gpioWrites == before);
    CHECK(registry.at(0).value());
  };
  for (const auto& value : {"\"true\"", "1", "0", "1.5", "null", "[]", "{}"})
    reject(make("PUT", endpoint, payload(boot, value)), 422, "INVALID_VALUE");
  reject(make("PUT", endpoint, payload("boot-old", "false")), 409, "DEVICE_RESTARTED");
  CHECK(api.response()["requestId"] == uuid);
  reject(make("PUT", endpoint, payload(boot, "false"), "text/plain"), 415, "UNSUPPORTED_MEDIA_TYPE");
  reject(make("PUT", "/api/v1/resources/missing/properties/on", payload(boot, "false")), 404, "RESOURCE_NOT_FOUND");
  reject(make("PUT", "/api/v1/resources/led.main/properties/missing", payload(boot, "false")), 404, "PROPERTY_NOT_FOUND");
  reject(make("PUT", "/api/v1/resources/led%2Fmain/properties/on", payload(boot, "false")), 400, "INVALID_REQUEST");
  reject(make("POST", "/api/v1/state"), 405, "METHOD_NOT_ALLOWED");
  CHECK(std::string(api.allowedMethod()) == "GET");
  reject(make("GET", endpoint), 405, "METHOD_NOT_ALLOWED");
  reject(make("GET", "/unknown"), 404, "RESOURCE_NOT_FOUND");
  for (const auto& invalid : {"{", "[]", "{}", "{\"requestId\":1}",
                             "{'value':false}", "{value:false}", "{\"value\":NaN}"})
    reject(make("PUT", endpoint, invalid), 400, "INVALID_REQUEST");
  reject(make("PUT", endpoint, payload(boot, "false") + "junk"), 400, "INVALID_REQUEST");
  reject(make("PUT", endpoint, payload(boot, "false") + "{}"), 400, "INVALID_REQUEST");
  reject(make("PUT", endpoint, payload(boot + "\\u0000hidden", "false")), 400, "INVALID_REQUEST");
  auto badUuid = payload(boot, "false"); badUuid.replace(badUuid.find(uuid), 36, "not-a-uuid");
  reject(make("PUT", endpoint, badUuid), 400, "INVALID_REQUEST");
  auto noValue = payload(boot, "false"); noValue.erase(noValue.find(",\"value\"")); noValue += "}";
  reject(make("PUT", endpoint, noValue), 400, "INVALID_REQUEST");
  auto padded = payload(boot, "true"); padded.resize(config::kMaxBody, ' ');
  auto boundary = make("PUT", endpoint, padded, "Application/Json; charset=utf-8");
  CHECK(boundary.complete && !boundary.errorStatus);
  api.handle(boundary); CHECK(api.status() == 200); CHECK(api.response()["revision"] == 1);
  auto oversize = make("PUT", endpoint, std::string(1025, ' '));
  CHECK(oversize.errorStatus == 413); CHECK(!oversize.complete);
  CHECK(oversize.body[0] == 0);
  for (const auto& headers : {"Content-Length: 1\r\nContent-Length: 1\r\n",
                              "Content-Length: -1\r\n", "Transfer-Encoding: chunked\r\n",
                              "Expect: 100-continue\r\n", "Bad Header: value\r\n"}) {
    CHECK(parse(std::string("PUT ") + endpoint + " HTTP/1.1\r\n" + headers + "\r\n").errorStatus == 400);
  }
  CHECK(parse("GET /api/v1/state HTTP/1.1\n\n").errorStatus == 400);
  CHECK(parse("GET /api/v1/state HTTP/1.1\r\nX: " + std::string(260, 'a') + "\r\n\r\n").errorStatus == 400);
  CHECK(!parse("PUT /api/v1/state HTTP/1.1\r\nContent-Length: 5\r\n\r\nabc").complete);
  auto embeddedNul = payload(boot, "false") + std::string(1, '\0') + "junk";
  CHECK(make("PUT", endpoint, embeddedNul).errorStatus == 400);
  failReadback = true;
  api.handle(make("PUT", endpoint, payload(boot, "false")));
  CHECK(api.status() == 500); CHECK(api.response()["error"]["code"] == "DRIVER_ERROR");
  CHECK(registry.at(0).value()); CHECK(pinLevels[2] == LOW);
  failReadback = false;
  api.handle(make("PUT", endpoint, payload(boot, "false")));
  CHECK(api.status() == 200); CHECK(api.response()["revision"] == 2); CHECK(pinLevels[2] == HIGH);
  ResourceRegistry restartedRegistry; CHECK(restartedRegistry.begin());
  Api restarted(restartedRegistry); restarted.begin(0x123456, 3, 4);
  restarted.handle(make("GET", "/api/v1/device"));
  CHECK(boot != restarted.response()["bootId"].as<const char*>());
  restarted.handle(make("GET", "/api/v1/state")); CHECK(restarted.response()["revision"] == 0);
  CHECK(restarted.response()["capabilitiesRevision"] == config::kCapabilitiesRevision);

  // Independent GPIO channels and numeric PWM commands, including driver failure.
  api.handle(make("PUT", "/api/v1/resources/led.red/properties/on", payload(boot, "true")));
  CHECK(api.status() == 200); CHECK(pinLevels[5] == HIGH && pinLevels[4] == LOW);
  const char* pwm = "/api/v1/resources/pwm.red/properties/level";
  const uint64_t beforePwm = registry.revision();
  // Android stores numeric targets as Double and serializes 25% as JSON 25.0.
  api.handle(make("PUT", pwm, payload(boot, "25.0")));
  CHECK(api.status() == 200); CHECK(api.response()["value"].is<int32_t>());
  CHECK(api.response()["value"] == 25 && registry.revision() == beforePwm + 1);
  CHECK(highMicros[14] == 250 && lowMicros[14] == 750);
  CHECK(highMicros[12] == 0); // green PWM unaffected
  api.handle(make("PUT", pwm, payload(boot, "25")));
  CHECK(registry.revision() == beforePwm + 1);
  for (const auto& invalid : {"-1", "101", "1.5", "0.5", "25.000001", "1e309", "\"25\"", "true", "null", "[]", "{}", "2147483648"}) {
    const unsigned beforeWrites = gpioWrites + waveformWrites;
    api.handle(make("PUT", pwm, payload(boot, invalid)));
    CHECK(api.status() == 422); CHECK(api.response()["error"]["code"] == "INVALID_VALUE");
    CHECK(gpioWrites + waveformWrites == beforeWrites && registry.revision() == beforePwm + 1);
  }
  failWaveform = true;
  api.handle(make("PUT", pwm, payload(boot, "50")));
  CHECK(api.status() == 500); CHECK(registry.find("pwm.red")->value() == 25);
  CHECK(highMicros[14] == 250 && registry.revision() == beforePwm + 1);
  failWaveform = false; failWaveformStop = true;
  api.handle(make("PUT", pwm, payload(boot, "100")));
  CHECK(api.status() == 500); CHECK(registry.find("pwm.red")->value() == 25);
  failWaveformStop = false;
  api.handle(make("PUT", pwm, payload(boot, "100")));
  CHECK(api.status() == 200 && pinLevels[14] == HIGH && highMicros[14] == 0);
  api.handle(make("PUT", pwm, payload(boot, "0")));
  CHECK(api.status() == 200 && pinLevels[14] == LOW && registry.find("pwm.red")->value() == 0);
  for (const auto& valid : {"0.0", "25.0", "50.0", "100.0", "2.5e1", "1e2", "-0.0"}) {
    const int32_t expected = static_cast<int32_t>(std::strtod(valid, nullptr));
    const int32_t previous = registry.find("pwm.red")->value();
    const uint64_t before = registry.revision();
    api.handle(make("PUT", pwm, payload(boot, valid)));
    CHECK(api.status() == 200 && api.response()["value"] == expected);
    CHECK(api.response()["value"].is<int32_t>());
    CHECK(registry.revision() == before + (previous != expected));
  }
  const unsigned beforeSensorWrite = gpioWrites + waveformWrites + analogReads;
  api.handle(make("PUT", "/api/v1/resources/sensor.pot/properties/raw", payload(boot, "12")));
  CHECK(api.status() == 422 && api.response()["error"]["code"] == "READ_ONLY");
  CHECK(gpioWrites + waveformWrites + analogReads == beforeSensorWrite);
  api.handle(make("PUT", "/api/v1/resources/sensor.pot/properties/voltage", payload(boot, "1.2")));
  CHECK(api.status() == 422 && api.response()["error"]["code"] == "READ_ONLY");
  CHECK(gpioWrites + waveformWrites + analogReads == beforeSensorWrite);

  // First sample, rate limit, unchanged values, invalid input and recovery.
  CHECK(!registry.find("sensor.pot")->available());
  uint64_t revision = registry.revision();
  testAnalogValue = 512; registry.sample(1000);
  CHECK(analogReads == 1 && registry.find("sensor.pot")->available());
  CHECK(registry.find("sensor.pot")->value() == 512 && registry.revision() == ++revision);
  CHECK(registry.find("sensor.pot")->voltageAvailable());
  CHECK(std::abs(registry.find("sensor.pot")->voltage() - 1.65) < 1e-9);
  api.handle(make("GET", "/api/v1/state"));
  CHECK(api.response()["resources"][5]["quality"]["voltage"] == "good");
  CHECK(std::abs(api.response()["resources"][5]["values"]["voltage"].as<double>() - 1.65) < 1e-9);
  std::string goodAnalogJson; serializeJson(api.response(), goodAnalogJson);
  registry.sample(1001); CHECK(analogReads == 1 && registry.revision() == revision);
  registry.sample(1250); CHECK(analogReads == 2 && registry.revision() == revision);
  testAnalogValue = 1023; registry.sample(1500); CHECK(registry.revision() == ++revision);
  testAnalogValue = -1; registry.sample(1750);
  CHECK(!registry.find("sensor.pot")->available() && registry.revision() == ++revision);
  api.handle(make("GET", "/api/v1/state"));
  CHECK(api.response()["resources"][5]["values"]["raw"].isNull());
  CHECK(api.response()["resources"][5]["quality"]["raw"] == "unavailable");
  registry.sample(2000); CHECK(registry.revision() == revision);
  testAnalogValue = 0; registry.sample(2250); CHECK(registry.revision() == ++revision);
  CHECK(registry.find("sensor.pot")->available() && registry.find("sensor.pot")->value() == 0);
  api.handle(make("GET", "/api/v1/state"));
  CHECK(api.response()["resources"][5]["values"]["raw"].is<int32_t>());
  CHECK(api.response()["resources"][5]["quality"]["raw"] == "good");
  CHECK(api.response()["capabilitiesRevision"] == config::kCapabilitiesRevision);

  // The former >1023 software cutoff is not a hardware overvoltage alarm.
  testAnalogValue = 1023; registry.sample(2500); CHECK(registry.revision() == ++revision);
  CHECK(std::abs(registry.find("sensor.pot")->voltage() - 3.297) < 1e-9);
  testAnalogValue = 1024; registry.sample(2750); CHECK(registry.revision() == ++revision);
  api.handle(make("GET", "/api/v1/state"));
  CHECK(api.response()["resources"][5]["values"]["raw"] == 1024);
  CHECK(api.response()["resources"][5]["quality"]["raw"] == "good");
  CHECK(api.response()["resources"][5]["quality"]["voltage"] == "good");
  CHECK(std::abs(api.response()["resources"][5]["values"]["voltage"].as<double>() - 3.3) < 1e-9);
  testAnalogValue = 1025; registry.sample(3000); CHECK(registry.revision() == ++revision);
  api.handle(make("GET", "/api/v1/state"));
  CHECK(api.response()["resources"][5]["values"]["raw"] == 1025);
  CHECK(api.response()["resources"][5]["quality"]["raw"] == "good");
  CHECK(api.response()["resources"][5]["values"]["voltage"].isNull());
  CHECK(api.response()["resources"][5]["quality"]["voltage"] == "unavailable");
  std::string aboveScaleJson; serializeJson(api.response(), aboveScaleJson);
  testAnalogValue = 65535; registry.sample(3250); CHECK(registry.revision() == ++revision);
  CHECK(registry.find("sensor.pot")->available() && !registry.find("sensor.pot")->voltageAvailable());
  testAnalogValue = 65536; registry.sample(3500); CHECK(registry.revision() == ++revision);
  CHECK(!registry.find("sensor.pot")->available());
  testAnalogValue = -1; registry.sample(3750); CHECK(registry.revision() == revision);
  testAnalogValue = 1024; registry.sample(4000); CHECK(registry.revision() == ++revision);
  CHECK(registry.find("sensor.pot")->available() && registry.find("sensor.pot")->voltageAvailable());

  // Bad registration never initializes a partial set of outputs.
  ResourceRegistry invalidRegistry;
  config::ResourceConfig invalid[2] = {config::kResources[1], config::kResources[2]};
  const unsigned beforeInvalid = gpioWrites;
  invalid[1].gpio = invalid[0].gpio; CHECK(!invalidRegistry.begin(invalid, 2));
  CHECK(gpioWrites == beforeInvalid);
  invalid[1] = invalid[0]; invalid[1].gpio = 4; CHECK(!invalidRegistry.begin(invalid, 2));
  CHECK(gpioWrites == beforeInvalid);
  invalid[1] = config::kResources[2]; invalid[1].propertyName = "\n";
  CHECK(!invalidRegistry.begin(invalid, 2)); CHECK(gpioWrites == beforeInvalid);
  invalid[0] = config::kResources[5]; invalid[1] = invalid[0]; invalid[1].id = "sensor.other";
  CHECK(!invalidRegistry.begin(invalid, 2)); CHECK(gpioWrites == beforeInvalid);
  invalid[0] = config::kResources[3]; invalid[0].step = 0;
  CHECK(!invalidRegistry.begin(invalid, 1)); CHECK(gpioWrites == beforeInvalid);
  invalid[0].step = 5; CHECK(invalidRegistry.begin(invalid, 1));
  Api stepped(invalidRegistry); stepped.begin(1, 1, 2);
  stepped.handle(make("PUT", pwm, payload(boot, "12")));
  CHECK(stepped.status() == 422 && invalidRegistry.revision() == 0);
  stepped.handle(make("PUT", pwm, payload(boot, "12.0")));
  CHECK(stepped.status() == 422 && invalidRegistry.revision() == 0);
  stepped.handle(make("PUT", pwm, payload(boot, "15.0")));
  CHECK(stepped.status() == 200 && stepped.response()["value"] == 15 && invalidRegistry.revision() == 1);
  ResourceRegistry emptyRegistry; CHECK(emptyRegistry.begin(nullptr, 0));
  Api empty(emptyRegistry); empty.begin(1, 1, 2); empty.handle(make("GET", "/api/v1/device"));
  CHECK(empty.response()["resources"].size() == 0);
  if (argc == 2) {
    const std::string directory = argv[1];
    std::ofstream(directory + "/device.json") << descriptionJson;
    std::ofstream(directory + "/state.json") << stateJson;
    std::ofstream(directory + "/receipt.json") << receiptJson;
    std::ofstream(directory + "/state-analog-good.json") << goodAnalogJson;
    std::ofstream(directory + "/state-analog-above-range.json") << aboveScaleJson;
  }
  std::cout << "PASS: " << checks << " protocol/HTTP/driver assertions\n";
}
