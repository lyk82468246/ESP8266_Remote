#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

inline bool validIdentifier(const char* text) {
  if (!text) return false;
  const size_t length = strlen(text);
  if (length == 0 || length > 48) return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = text[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
  }
  return true;
}

// Count Unicode scalar values, not UTF-8 bytes; reject malformed encodings and C0/C1.
// Whitespace matches Kotlin Char.isWhitespace / Unicode space separators.
inline bool validDisplayName(const char* text) {
  if (!text) return false;
  const unsigned char* p = reinterpret_cast<const unsigned char*>(text);
  size_t count = 0;
  bool visible = false;
  while (*p) {
    const unsigned char first = *p++;
    uint32_t cp = first, minimum = 0;
    unsigned continuation = 0;
    if (first >= 0xc2 && first <= 0xdf) { cp = first & 0x1f; continuation = 1; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { cp = first & 0x0f; continuation = 2; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { cp = first & 0x07; continuation = 3; minimum = 0x10000; }
    else if (first >= 0x80) return false;
    for (unsigned i = 0; i < continuation; ++i) {
      if ((*p & 0xc0) != 0x80) return false;
      cp = (cp << 6) | (*p++ & 0x3f);
    }
    if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) ||
        cp < 0x20 || (cp >= 0x7f && cp <= 0x9f) || ++count > 64) return false;
    const bool whitespace = cp == 0x20 || cp == 0xa0 || cp == 0x1680 ||
        (cp >= 0x2000 && cp <= 0x200a) || cp == 0x2028 || cp == 0x2029 ||
        cp == 0x202f || cp == 0x205f || cp == 0x3000;
    visible |= !whitespace;
  }
  return count > 0 && visible;
}
