#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
// Configurable time sources and persistent records by axelaberg91.
#include <Preferences.h>
#include <stdint.h>
#include <string.h>

namespace watchsync {
constexpr size_t NTP_HOST_MAX = 253;
constexpr char DEFAULT_NTP_PRIMARY[] = "pool.ntp.org";
constexpr char DEFAULT_NTP_SECONDARY[] = "time.nist.gov";

inline bool validNtpHost(const char *host, size_t size) {
  if (!host || !size || size > NTP_HOST_MAX) return false;
  bool numeric = true;
  size_t label = 0;
  for (size_t i = 0; i < size; ++i) {
    const char c = host[i];
    if (c == '.') {
      if (!label || host[i - 1] == '-') return false;
      label = 0;
    } else {
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-')) return false;
      if ((!label && c == '-') || ++label > 63) return false;
      if (c < '0' || c > '9') numeric = false;
    }
  }
  if (!label || host[size - 1] == '-') return false;
  if (!numeric) return true;
  unsigned parts = 0, value = 0, digits = 0;
  for (size_t i = 0; i <= size; ++i) {
    if (i == size || host[i] == '.') {
      if (!digits || value > 255) return false;
      ++parts; value = digits = 0;
    } else {
      if (digits && value == 0) return false;
      value = value * 10 + unsigned(host[i] - '0');
      if (++digits > 3) return false;
    }
  }
  // Unicast IPv4 only; loopback, unspecified, multicast and broadcast are unsuitable.
  unsigned first = 0;
  for (size_t i = 0; i < size && host[i] != '.'; ++i) first = first * 10 + unsigned(host[i] - '0');
  return parts == 4 && first > 0 && first < 224 && first != 127;
}

struct NtpRecord {
  uint8_t version = 1, custom = 0;
  char primary[NTP_HOST_MAX + 1] = {}, secondary[NTP_HOST_MAX + 1] = {};
  bool valid() const {
    if (version != 1 || custom > 1) return false;
    const size_t a = strnlen(primary, sizeof(primary)), b = strnlen(secondary, sizeof(secondary));
    if (a == sizeof(primary) || b == sizeof(secondary)) return false;
    return custom ? validNtpHost(primary, a) && (!b || validNtpHost(secondary, b)) : !a && !b;
  }
  const char *first() const { return custom ? primary : DEFAULT_NTP_PRIMARY; }
  const char *second() const { return custom ? secondary : DEFAULT_NTP_SECONDARY; }
};
static_assert(sizeof(NtpRecord) == 510, "Keep the versioned NTP record layout stable");

class NtpConfiguration {
 public:
  const NtpRecord &record() const { return record_; }
  bool error() const { return error_; }
  void load() {
    Preferences storage;
    if (!storage.begin("watchsync", true)) return;
    if (storage.isKey("timesource")) {
      NtpRecord loaded;
      if (storage.getBytesLength("timesource") != sizeof(loaded) ||
          storage.getBytes("timesource", &loaded, sizeof(loaded)) != sizeof(loaded) || !loaded.valid()) error_ = true;
      else record_ = loaded;
    }
    storage.end();
  }
  // Call while the radio acknowledges its storage pause. Publish only after commit.
  bool save(const NtpRecord &next) {
    if (!next.valid()) return false;
    Preferences storage;
    if (!storage.begin("watchsync", false)) return false;
    const bool saved = storage.putBytes("timesource", &next, sizeof(next)) == sizeof(next);
    storage.end();
    if (saved) { record_ = next; error_ = false; }
    return saved;
  }
 private:
  NtpRecord record_;
  bool error_ = false;
};
}  // namespace watchsync
