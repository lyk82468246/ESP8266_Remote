#pragma once
#include <ArduinoJson.h>
#include <cmath>
#include "HttpRequest.h"
#include "Resources.h"
#include "StrictJson.h"

class JsonBodyReader {
 public:
  explicit JsonBodyReader(const char* body) : cursor(body) {}
  int read() { return *cursor ? static_cast<unsigned char>(*cursor++) : -1; }
  size_t readBytes(char* destination, size_t length) {
    size_t count = 0;
    while (count < length && *cursor) destination[count++] = *cursor++;
    return count;
  }
  const char* cursor;
};

class Api {
 public:
  explicit Api(ResourceRegistry& registry) : registry_(registry) {}
  void begin(uint32_t chipId, uint32_t entropy1, uint32_t entropy2) {
    snprintf(deviceId_, sizeof(deviceId_), "esp8266-%06lx", static_cast<unsigned long>(chipId));
    snprintf(bootId_, sizeof(bootId_), "boot-%08lx%08lx",
             static_cast<unsigned long>(entropy1), static_cast<unsigned long>(entropy2));
  }
  int status() const { return status_; }
  const char* deviceId() const { return deviceId_; }
  const char* allowedMethod() const { return allowedMethod_; }
  JsonDocument& response() { return response_; }
  void error(int status, const char* code, const char* message, const char* requestId = nullptr) {
    response_.clear(); status_ = status;
    response_["protocolVersion"] = 1;
    response_["requestId"] = requestId;
    JsonObject detail = response_.createNestedObject("error");
    detail["code"] = code; detail["message"] = message;
  }
  void handle(const HttpRequest& request) {
    allowedMethod_ = nullptr;
    response_.clear(); status_ = 200;
    if (!strcmp(request.path, "/api/v1/device")) {
      if (strcmp(request.method, "GET")) { methodError("GET"); return; }
      describe(); return;
    }
    if (!strcmp(request.path, "/api/v1/state")) {
      if (strcmp(request.method, "GET")) { methodError("GET"); return; }
      snapshot(); return;
    }
    constexpr const char* prefix = "/api/v1/resources/";
    if (strncmp(request.path, prefix, strlen(prefix))) {
      error(404, "RESOURCE_NOT_FOUND", "Unknown API path"); return;
    }
    const char* start = request.path + strlen(prefix);
    const char* separator = strstr(start, "/properties/");
    if (!separator || static_cast<size_t>(separator - start) > 48 || separator == start) {
      error(400, "INVALID_REQUEST", "Invalid resource path"); return;
    }
    char resourceId[49] = {};
    memcpy(resourceId, start, separator - start);
    const char* propertyId = separator + strlen("/properties/");
    if (!validIdentifier(resourceId) || !validIdentifier(propertyId)) {
      error(400, "INVALID_REQUEST", "Invalid resource or property ID"); return;
    }
    if (strcmp(request.method, "PUT")) { methodError("PUT"); return; }
    write(request, resourceId, propertyId);
  }

 private:
  ResourceRegistry& registry_;
  StaticJsonDocument<4096> response_;
  StaticJsonDocument<4096> input_;
  char deviceId_[24] = {}, bootId_[32] = {};
  int status_ = 200;
  const char* allowedMethod_ = nullptr;
  void methodError(const char* allowed) {
    allowedMethod_ = allowed;
    error(405, "METHOD_NOT_ALLOWED", "Method not supported for this endpoint");
  }
  void identity() {
    response_["protocolVersion"] = 1;
    response_["deviceId"] = deviceId_;
    response_["bootId"] = bootId_;
  }
  void describe() {
    identity(); response_["name"] = config::kDeviceName;
    response_["capabilitiesRevision"] = config::kCapabilitiesRevision;
    response_["firmwareVersion"] = config::kFirmwareVersion;
    JsonArray resources = response_.createNestedArray("resources");
    for (size_t i = 0; i < registry_.size(); ++i) {
      const auto& spec = registry_.at(i).spec();
      JsonObject resource = resources.createNestedObject();
      resource["id"] = spec.id; resource["name"] = spec.name;
      const bool boolean = spec.type == config::ResourceType::Binary;
      const bool writable = spec.type != config::ResourceType::Analog;
      resource["kind"] = boolean ? "binary-output" : (writable ? "range-output" : "sensor");
      JsonArray properties = resource.createNestedArray("properties");
      JsonObject property = properties.createNestedObject();
      property["id"] = spec.propertyId; property["name"] = spec.propertyName;
      property["type"] = boolean ? "boolean" : "integer";
      property["readable"] = true; property["writable"] = writable;
      if (!boolean) {
        property["min"] = spec.minimum; property["max"] = spec.maximum;
        property["step"] = spec.step; property["unit"] = spec.unit;
      }
      if (spec.type == config::ResourceType::Analog) {
        JsonObject voltage = properties.createNestedObject();
        voltage["id"] = config::kVoltagePropertyId; voltage["name"] = config::kVoltagePropertyName;
        voltage["type"] = "number"; voltage["readable"] = true; voltage["writable"] = false;
        voltage["min"] = 0; voltage["max"] = config::kAdcFullScaleVolts; voltage["unit"] = "V";
      }
    }
  }
  void snapshot() {
    identity(); response_["revision"] = registry_.revision();
    response_["capabilitiesRevision"] = config::kCapabilitiesRevision;
    JsonArray resources = response_.createNestedArray("resources");
    for (size_t i = 0; i < registry_.size(); ++i) {
      auto& output = registry_.at(i);
      JsonObject resource = resources.createNestedObject();
      resource["id"] = output.spec().id;
      JsonObject values = resource.createNestedObject("values");
      if (!output.available()) values[output.spec().propertyId] = nullptr;
      else if (output.boolean()) values[output.spec().propertyId] = static_cast<bool>(output.value());
      else values[output.spec().propertyId] = output.value();
      JsonObject quality = resource.createNestedObject("quality");
      quality[output.spec().propertyId] = output.available() ? "good" : "unavailable";
      if (output.spec().type == config::ResourceType::Analog) {
        if (output.voltageAvailable()) values[config::kVoltagePropertyId] = output.voltage();
        else values[config::kVoltagePropertyId] = nullptr;
        quality[config::kVoltagePropertyId] = output.voltageAvailable() ? "good" : "unavailable";
      }
    }
  }
  static bool validUuid(const char* uuid) {
    if (!uuid || strlen(uuid) != 36) return false;
    for (size_t i = 0; i < 36; ++i) {
      if (i == 8 || i == 13 || i == 18 || i == 23) { if (uuid[i] != '-') return false; }
      else if (!isxdigit(static_cast<unsigned char>(uuid[i]))) return false;
    }
    return true;
  }
  void write(const HttpRequest& request, const char* resourceId, const char* propertyId) {
    if (!request.jsonMediaType) {
      error(415, "UNSUPPORTED_MEDIA_TYPE", "Content-Type must be application/json"); return;
    }
    input_.clear(); JsonBodyReader reader(request.body);
    if (!StrictJson::valid(request.body)) {
      error(400, "INVALID_REQUEST", "Body is not valid JSON"); return;
    }
    const auto parseError = deserializeJson(input_, reader, DeserializationOption::NestingLimit(8));
    while (*reader.cursor && isspace(static_cast<unsigned char>(*reader.cursor))) ++reader.cursor;
    if (parseError || *reader.cursor || !input_.is<JsonObject>()) {
      error(400, "INVALID_REQUEST", "Body must be one JSON object"); return;
    }
    const char* requestId = input_["requestId"].is<const char*>() ? input_["requestId"].as<const char*>() : nullptr;
    if (!validUuid(requestId) || input_["requestId"].as<JsonString>().size() != 36) {
      error(400, "INVALID_REQUEST", "requestId must be a UUID"); return;
    }
    const char* expectedBoot = input_["expectedBootId"].is<const char*>() ? input_["expectedBootId"].as<const char*>() : nullptr;
    if (!expectedBoot || !*expectedBoot ||
        input_["expectedBootId"].as<JsonString>().size() != strlen(expectedBoot) ||
        !input_.containsKey("value")) {
      error(400, "INVALID_REQUEST", "Missing expectedBootId or value", requestId); return;
    }
    Peripheral* output = registry_.find(resourceId);
    if (!output) { error(404, "RESOURCE_NOT_FOUND", "Resource does not exist", requestId); return; }
    if (strcmp(propertyId, output->spec().propertyId) &&
        !(output->spec().type == config::ResourceType::Analog && !strcmp(propertyId, config::kVoltagePropertyId))) {
      error(404, "PROPERTY_NOT_FOUND", "Property does not exist", requestId); return;
    }
    if (strcmp(expectedBoot, bootId_)) {
      error(409, "DEVICE_RESTARTED", "Read device and state again", requestId); return;
    }
    if (!output->writable()) {
      error(422, "READ_ONLY", "Sensor property is read-only", requestId); return;
    }
    const auto& spec = output->spec();
    int32_t target = 0;
    if (output->boolean()) {
      if (!input_["value"].is<bool>()) {
        error(422, "INVALID_VALUE", "Value must be a JSON boolean", requestId); return;
      }
      target = static_cast<int32_t>(input_["value"].as<bool>());
    } else {
      if (!input_["value"].is<double>()) {
        error(422, "INVALID_VALUE", "Value must be a JSON number", requestId); return;
      }
      const double number = input_["value"].as<double>();
      // JSON 25 and 25.0 both represent an integer value. Android uses Double.
      // Validate before narrowing so fractional, non-finite and large values cannot be coerced.
      if (!std::isfinite(number) || std::trunc(number) != number ||
          number < spec.minimum || number > spec.maximum) {
        error(422, "INVALID_VALUE", "Value must be a finite whole number within range", requestId); return;
      }
      target = static_cast<int32_t>(number);
    }
    if (target < spec.minimum || target > spec.maximum || (target - spec.minimum) % spec.step != 0) {
      error(422, "INVALID_VALUE", "Value outside range or step", requestId); return;
    }
    if (!registry_.write(*output, target)) {
      error(500, "DRIVER_ERROR", "Output driver could not confirm configuration", requestId); return;
    }
    identity(); response_["requestId"] = requestId;
    response_["resourceId"] = spec.id; response_["propertyId"] = spec.propertyId;
    if (output->boolean()) response_["value"] = static_cast<bool>(output->value());
    else response_["value"] = output->value();
    response_["revision"] = registry_.revision();
  }
};
