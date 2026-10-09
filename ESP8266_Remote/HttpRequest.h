#pragma once
#include "Config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// Incremental, bounded HTTP/1.x parser. One request per connection, no chunked bodies.
// Body size is checked at header time, before allocation or JSON deserialization.
class HttpRequest {
 public:
  char method[16] = {};
  char path[192] = {};
  char body[config::kMaxBody + 1] = {};
  bool jsonMediaType = false;
  bool hasLength = false;
  bool complete = false;
  int errorStatus = 0;
  const char* errorCode = "INVALID_REQUEST";

  void feed(char c) {
    if (complete || errorStatus) return;
    if (inBody_) {
      if (c == '\0') { fail(400); return; }
      body[bodyBytes_++] = c;
      if (bodyBytes_ == contentLength_) { body[bodyBytes_] = '\0'; complete = true; }
      return;
    }
    if (++headerBytes_ > 2048) { fail(400); return; }
    if (c == '\n') {
      if (lineBytes_ == 0 || line_[lineBytes_ - 1] != '\r') { fail(400); return; }
      line_[--lineBytes_] = '\0';
      if (!requestLine_) parseRequestLine();
      else if (!lineBytes_) {
        inBody_ = true;
        if (!contentLength_) complete = true;
      } else parseHeader();
      lineBytes_ = 0;
    } else {
      if (c == '\0' || lineBytes_ >= sizeof(line_) - 1) { fail(400); return; }
      line_[lineBytes_++] = c;
    }
  }

 private:
  char line_[256] = {};
  size_t lineBytes_ = 0, headerBytes_ = 0, bodyBytes_ = 0, contentLength_ = 0;
  bool requestLine_ = false, inBody_ = false, hasType_ = false;
  void fail(int status) {
    errorStatus = status;
    if (status == 413) errorCode = "PAYLOAD_TOO_LARGE";
  }
  void parseRequestLine() {
    char* space = strchr(line_, ' ');
    if (!space) { fail(400); return; }
    *space++ = '\0';
    char* version = strchr(space, ' ');
    if (!version) { fail(400); return; }
    *version++ = '\0';
    if (!*line_ || strlen(line_) >= sizeof(method) || strlen(space) >= sizeof(path) ||
        *space != '/' || (strcmp(version, "HTTP/1.1") && strcmp(version, "HTTP/1.0"))) {
      fail(400); return;
    }
    for (const char* p = line_; *p; ++p) if (*p < 'A' || *p > 'Z') { fail(400); return; }
    for (const char* p = space; *p; ++p) if (*p <= 32 || *p == 127) { fail(400); return; }
    strcpy(method, line_); strcpy(path, space); requestLine_ = true;
  }
  void parseHeader() {
    char* value = strchr(line_, ':');
    if (!value || value == line_) { fail(400); return; }
    *value++ = '\0';
    for (const char* p = line_; *p; ++p)
      if (!(isalnum(static_cast<unsigned char>(*p)) || strchr("!#$%&'*+-.^_`|~", *p))) {
        fail(400); return;
      }
    while (*value == ' ' || *value == '\t') ++value;
    size_t length = strlen(value);
    while (length && (value[length - 1] == ' ' || value[length - 1] == '\t')) value[--length] = '\0';
    for (size_t i = 0; i < length; ++i)
      if ((static_cast<unsigned char>(value[i]) < 32 && value[i] != '\t') || value[i] == 127) {
        fail(400); return;
      }
    if (!strcasecmp(line_, "Content-Length")) {
      if (hasLength || !length) { fail(400); return; }
      hasLength = true;
      for (size_t i = 0; i < length; ++i) {
        if (value[i] < '0' || value[i] > '9') { fail(400); return; }
        contentLength_ = contentLength_ * 10 + (value[i] - '0');
        if (contentLength_ > config::kMaxBody) { fail(413); return; }
      }
    } else if (!strcasecmp(line_, "Transfer-Encoding")) {
      fail(400);
    } else if (!strcasecmp(line_, "Content-Type")) {
      if (hasType_) { fail(400); return; }
      hasType_ = true;
      char* parameter = strchr(value, ';');
      if (parameter) *parameter = '\0';
      length = strlen(value);
      while (length && value[length - 1] == ' ') value[--length] = '\0';
      jsonMediaType = !strcasecmp(value, "application/json");
    } else if (!strcasecmp(line_, "Expect")) {
      // Never wait for a body whose sender is waiting for 100 Continue.
      fail(400);
    }
  }
};
