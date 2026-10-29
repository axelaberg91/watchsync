#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
// Bounded firmware image checks by axelaberg91.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace watchsync {
// An accidental-upload check, not a cryptographic authenticity guarantee.
static constexpr char OTA_TARGET[] = "WATCHSYNC-OTA:ATOM-LITE:GPIO25:v1";
static constexpr size_t OTA_MIN_IMAGE_SIZE = 4096;
static constexpr uint32_t OTA_TICKET_MS = 30000;
static constexpr uint32_t OTA_TRANSFER_MS = 120000;

inline bool otaElapsed(uint32_t now, uint32_t since, uint32_t limit) {
  return uint32_t(now - since) >= limit;
}

struct FirmwareImageGuard {
  // ESP image header, first segment header, and the app descriptor magic.
  uint8_t prefix[36] = {};
  size_t received = 0;
  size_t matched = 0;
  bool targetFound = false;

  bool headerReady() const { return received >= sizeof(prefix); }
  bool validHeader() const {
    return headerReady() && prefix[0] == 0xe9 && prefix[1] > 0 && prefix[1] <= 16 &&
        prefix[12] == 0 && prefix[13] == 0 && prefix[23] == 1 &&
        prefix[32] == 0x32 && prefix[33] == 0x54 && prefix[34] == 0xcd && prefix[35] == 0xab;
  }

  bool feed(const uint8_t *bytes, size_t count, size_t expected) {
    if (received > expected || count > expected - received) return false;
    for (size_t i = 0; i < count; ++i) {
      const uint8_t b = bytes[i];
      if (received < sizeof(prefix)) prefix[received] = b;
      ++received;
      if (!targetFound) {
        // The marker contains W only at its start, so this handles all overlaps.
        matched = b == uint8_t(OTA_TARGET[matched]) ? matched + 1 : (b == 'W' ? 1 : 0);
        if (matched == sizeof(OTA_TARGET) - 1) targetFound = true;
      }
    }
    return !headerReady() || validHeader();
  }

  bool complete(size_t expected) const {
    return received == expected && validHeader() && targetFound;
  }
};
}  // namespace watchsync
