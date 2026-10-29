#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
// Durable key-value simulation with interrupted-write injection by axelaberg91.
#include <stdint.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

namespace fakeNVS {
using Bytes = std::vector<uint8_t>;
inline std::map<std::string, std::map<std::string, Bytes>> data;
inline int writesLeft = -1;
inline int writes = 0;
inline bool writable() {
  if (writesLeft == 0) return false;
  if (writesLeft > 0) --writesLeft;
  ++writes;
  return true;
}
}

class Preferences {
 public:
  bool begin(const char *name, bool readOnly = false) {
    space_ = name; readOnly_ = readOnly;
    if (!fakeNVS::data.count(space_)) {
      if (readOnly || !fakeNVS::writable()) return false;
      fakeNVS::data[space_] = {};
    }
    return true;
  }
  void end() {}
  bool isKey(const char *key) { return fakeNVS::data[space_].count(key); }
  bool clear() {
    if (readOnly_ || !fakeNVS::writable()) return false;
    fakeNVS::data[space_].clear(); return true;
  }
  bool remove(const char *key) {
    if (readOnly_ || !fakeNVS::writable()) return false;
    return fakeNVS::data[space_].erase(key) != 0;
  }
  size_t putBytes(const char *key, const void *bytes, size_t size) {
    if (readOnly_ || !fakeNVS::writable()) return 0;
    const uint8_t *p = static_cast<const uint8_t *>(bytes);
    fakeNVS::data[space_][key] = fakeNVS::Bytes(p, p + size); return size;
  }
  size_t putString(const char *key, const char *value) {
    return putBytes(key, value, strlen(value) + 1) ? strlen(value) : 0;
  }
  size_t getString(const char *key, char *out, size_t capacity) {
    if (!isKey(key)) return 0;
    const auto &bytes = fakeNVS::data[space_][key];
    if (bytes.size() > capacity) return 0;
    memcpy(out, bytes.data(), bytes.size()); return bytes.size();
  }
  size_t putBool(const char *key, bool value) { return putBytes(key, &value, sizeof(value)); }
  bool getBool(const char *key, bool fallback) {
    if (!isKey(key) || fakeNVS::data[space_][key].size() != sizeof(bool)) return fallback;
    return fakeNVS::data[space_][key][0] != 0;
  }
 private:
  std::string space_;
  bool readOnly_ = true;
};
