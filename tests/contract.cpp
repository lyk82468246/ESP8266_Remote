#include <cstdlib>
#include <iostream>
#include <string>
#include "Api.h"

int pinLevels[17] = {};
unsigned gpioWrites = 0;
bool failReadback = false;
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

int main() {
  ResourceRegistry registry;
  CHECK(registry.begin());
  CHECK(pinLevels[2] == HIGH);
  CHECK(!registry.at(0).value());
  Api api(registry); api.begin(0x123456, 1, 2);
  api.handle(make("GET", "/api/v1/device"));
  CHECK(api.status() == 200);
  CHECK(api.response()["protocolVersion"] == 1);
  CHECK(api.response()["resources"].size() == config::kOutputCount);
  CHECK(api.response()["resources"][0]["properties"][0]["type"] == "boolean");
  CHECK(api.response()["resources"][0]["properties"][0]["writable"] == true);
  const std::string boot = api.response()["bootId"].as<const char*>();
  api.handle(make("GET", "/api/v1/state"));
  CHECK(api.response()["revision"] == 0);
  CHECK(api.response()["resources"][0]["values"]["on"] == false);
  CHECK(api.response()["resources"][0]["quality"]["on"] == "good");
  api.handle(make("PUT", endpoint, payload(boot, "true")));
  CHECK(api.status() == 200);
  CHECK(pinLevels[2] == LOW);
  CHECK(api.response()["value"] == true);
  CHECK(api.response()["requestId"] == uuid);
  CHECK(api.response()["revision"] == 1);
  api.handle(make("PUT", endpoint, payload(boot, "true")));
  CHECK(api.response()["revision"] == 1);
  CHECK(registry.at(0).value());
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
  Api restarted(registry); restarted.begin(0x123456, 3, 4);
  restarted.handle(make("GET", "/api/v1/device"));
  CHECK(boot != restarted.response()["bootId"].as<const char*>());
  restarted.handle(make("GET", "/api/v1/state")); CHECK(restarted.response()["revision"] == 0);
  std::cout << "PASS: " << checks << " protocol/HTTP/driver assertions\n";
}
