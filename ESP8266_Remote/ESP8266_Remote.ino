#include <ESP8266WiFi.h>
extern "C" {
#include <user_interface.h>
}
#include "Api.h"
#include "NetworkService.h"

ResourceRegistry registry;
Api api(registry);
NetworkService network;
WiFiServer server(config::kPort);
WiFiClient client;
HttpRequest request;
bool ready = false, sending = false;
uint32_t acceptedAt = 0, lastByteAt = 0;
char responseBody[config::kMaxResponse + 1];
size_t responseLength = 0, responseOffset = 0;
char responseHeader[384];
size_t headerLength = 0, headerOffset = 0;

static const char* reason(int status) {
  switch (status) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 422: return "Unprocessable Entity";
    case 503: return "Service Unavailable";
    default: return "Internal Server Error";
  }
}

static void prepareResponse() {
  if (api.response().overflowed() || measureJson(api.response()) > config::kMaxResponse)
    api.error(500, "DRIVER_ERROR", "Response capacity exceeded");
  responseLength = serializeJson(api.response(), responseBody, sizeof(responseBody));
  const char* allowed = api.status() == 405 ? api.allowedMethod() : nullptr;
  headerLength = snprintf(responseHeader, sizeof(responseHeader),
      "HTTP/1.1 %d %s\r\nContent-Type: application/json; charset=utf-8\r\n"
      "Cache-Control: no-store\r\nConnection: close\r\nContent-Length: %u\r\n%s%s%s\r\n",
      api.status(), reason(api.status()), static_cast<unsigned>(responseLength),
      allowed ? "Allow: " : "", allowed ? allowed : "", allowed ? "\r\n" : "");
  responseOffset = headerOffset = 0;
  sending = true; lastByteAt = millis();
}

void setup() {
  Serial.begin(115200); Serial.println();
  if (!registry.begin()) {
    Serial.println("Invalid output configuration (name, ID, GPIO or duplicate pin). API disabled.");
    return;
  }
  api.begin(ESP.getChipId(), os_random(), os_random());
  if (!network.begin(api.deviceId())) { Serial.println("Network setup failed. API disabled."); return; }
  server.begin(); server.setNoDelay(true); ready = true;
}

void loop() {
  // Sampling uses a fixed cadence and cached results; HTTP handlers never wait on a sensor.
  registry.sample(millis());
  if (!ready) { delay(1); return; }
  network.update();
  if (!client) {
    client = server.accept();
    if (!client) { delay(1); return; }
    client.setTimeout(100); request = HttpRequest(); sending = false;
    acceptedAt = lastByteAt = millis();
  }
  if (sending) {
    // Small writes avoid monopolizing the main loop for a slow reader.
    const int available = client.availableForWrite();
    if (available > 0) {
      const size_t room = static_cast<size_t>(available);
      const bool header = headerOffset < headerLength;
      const char* buffer = header ? responseHeader : responseBody;
      size_t& offset = header ? headerOffset : responseOffset;
      const size_t length = header ? headerLength : responseLength;
      const size_t count = min(static_cast<size_t>(256), min(room, length - offset));
      const size_t written = client.write(reinterpret_cast<const uint8_t*>(buffer + offset), count);
      offset += written;
      if (written) lastByteAt = millis();
    }
    if (headerOffset == headerLength && responseOffset == responseLength) {
      client.stop(100); client = WiFiClient(); sending = false;
    } else if (millis() - lastByteAt > config::kIdleTimeoutMs ||
               millis() - acceptedAt > config::kRequestTimeoutMs + 2000) {
      client.stop(); client = WiFiClient(); sending = false;
    }
  } else {
    for (size_t budget = 0; budget < 256 && client.available(); ++budget) {
      request.feed(static_cast<char>(client.read())); lastByteAt = millis();
      if (request.errorStatus || request.complete) break;
    }
    if (request.errorStatus) {
      api.error(request.errorStatus, request.errorCode, "Invalid or oversized HTTP request");
      prepareResponse();
    } else if (request.complete) {
      api.handle(request); prepareResponse();
    } else if (millis() - lastByteAt > config::kIdleTimeoutMs ||
               millis() - acceptedAt > config::kRequestTimeoutMs) {
      api.error(400, "INVALID_REQUEST", "Incomplete request timed out"); prepareResponse();
    }
  }
  delay(0);
}
