#pragma once
#include <ctype.h>
#include <string.h>
#include <initializer_list>

// ArduinoJson accepts some non-JSON extensions. Validate the wire grammar first.
class StrictJson {
 public:
  static bool valid(const char* body) {
    StrictJson parser(body);
    if (!parser.value(0)) return false;
    parser.space();
    return *parser.p_ == '\0';
  }
 private:
  explicit StrictJson(const char* text) : p_(text) {}
  const char* p_;
  void space() { while (*p_ == ' ' || *p_ == '\t' || *p_ == '\r' || *p_ == '\n') ++p_; }
  bool take(char c) { space(); if (*p_ != c) return false; ++p_; return true; }
  bool string() {
    if (!take('"')) return false;
    while (*p_ && *p_ != '"') {
      if (static_cast<unsigned char>(*p_) < 32) return false;
      if (*p_++ == '\\') {
        if (!*p_) return false;
        const char escape = *p_++;
        if (escape == 'u') {
          for (int i = 0; i < 4; ++i) {
            if (!isxdigit(static_cast<unsigned char>(*p_))) return false;
            ++p_;
          }
        } else if (!strchr("\"\\/bfnrt", escape)) return false;
      }
    }
    if (*p_ != '"') return false;
    ++p_; return true;
  }
  bool number() {
    if (*p_ == '-') ++p_;
    if (*p_ == '0') ++p_;
    else {
      if (*p_ < '1' || *p_ > '9') return false;
      do { ++p_; } while (*p_ >= '0' && *p_ <= '9');
    }
    if (*p_ == '.') {
      ++p_;
      if (*p_ < '0' || *p_ > '9') return false;
      do { ++p_; } while (*p_ >= '0' && *p_ <= '9');
    }
    if (*p_ == 'e' || *p_ == 'E') {
      ++p_; if (*p_ == '+' || *p_ == '-') ++p_;
      if (*p_ < '0' || *p_ > '9') return false;
      do { ++p_; } while (*p_ >= '0' && *p_ <= '9');
    }
    return true;
  }
  bool value(unsigned depth) {
    space(); if (depth > 8) return false;
    if (*p_ == '"') return string();
    if (*p_ == '{' || *p_ == '[') {
      const bool object = *p_++ == '{';
      const char end = object ? '}' : ']';
      if (take(end)) return true;
      do {
        if (object && (!string() || !take(':'))) return false;
        if (!value(depth + 1)) return false;
        if (take(end)) return true;
      } while (take(','));
      return false;
    }
    for (const char* literal : {"true", "false", "null"}) {
      const size_t length = strlen(literal);
      if (!strncmp(p_, literal, length)) { p_ += length; return true; }
    }
    return number();
  }
};
