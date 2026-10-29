#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
// Persistent device identity and restart-safe factory reset by axelaberg91.
#include <Preferences.h>
#include <stdint.h>
#include <string.h>

namespace watchsync {
inline bool validHostname(const char *name, size_t size) {
  if (!name || size == 0 || size > 32 || name[0] == '-' || name[size - 1] == '-') return false;
  for (size_t i = 0; i < size; ++i)
    if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= '0' && name[i] <= '9') || name[i] == '-')) return false;
  return true;
}

// Preserve the existing Wi-Fi record layout so OTA upgrades retain credentials.
struct StoredWifiNetwork { uint32_t magic; char ssid[33]; char psk[65]; };
static constexpr uint32_t NETWORK_MAGIC = 0x57494631;
static constexpr uint32_t PORTAL_MAGIC = 0x57494630;

class DeviceConfiguration {
 public:
  bool begin() {
    Preferences storage;
    if (!storage.begin("watchsyncdev", false)) return false;
    const bool reset = storage.getBool("reset", false);
    storage.end();
    if (reset && !completeReset()) return false;
    if (!storage.begin("watchsyncdev", true)) return false;
    char stored[33] = {};
    const size_t bytes = storage.getString("hostname", stored, sizeof(stored));
    storage.end();
    if (bytes > 1 && bytes <= sizeof(stored) && stored[bytes - 1] == 0 && validHostname(stored, bytes - 1))
      memcpy(active_, stored, bytes);
    else strcpy(active_, "watchsync");
    strcpy(next_, active_);
    return true;
  }

  const char *hostname() const { return active_; }
  const char *nextHostname() const { return next_; }
  bool pending() const { return strcmp(active_, next_) != 0; }

  // Call only while the radio has acknowledged its storage pause.
  bool saveHostname(const char *name, size_t size) {
    if (!validHostname(name, size)) return false;
    char value[33] = {};
    memcpy(value, name, size);
    if (strcmp(value, next_) == 0) return true;
    Preferences storage;
    if (!storage.begin("watchsyncdev", false)) return false;
    const bool saved = storage.putString("hostname", value) == size;
    storage.end();
    if (saved) memcpy(next_, value, size + 1);
    return saved;
  }

  bool requestFactoryReset() {
    Preferences storage;
    if (!storage.begin("watchsyncdev", false)) return false;
    const bool saved = storage.putBool("reset", true) == sizeof(bool);
    storage.end();
    return saved;
  }

 private:
  char active_[33] = "watchsync", next_[33] = "watchsync";

  static bool clearNamespace(const char *name) {
    Preferences storage;
    if (!storage.begin(name, false)) return false;
    const bool cleared = storage.clear();
    storage.end();
    return cleared;
  }

  static bool completeReset() {
    // Keep the durable marker until every operation succeeds. A power loss retries
    // this idempotent cleanup before settings, Wi-Fi or the radio are initialized.
    const char *names[] = {"watchsync", "clocksync", "watchsyncwifi", "clocksyncwifi"};
    for (const char *name : names)
      if (!clearNamespace(name)) return false;
    Preferences storage;
    if (!storage.begin("watchsyncwifi", false)) return false;
    StoredWifiNetwork portal = {};
    portal.magic = PORTAL_MAGIC;
    // Suppress compiled credentials as well as legacy Wi-Fi migration.
    const bool saved = storage.putBytes("network", &portal, sizeof(portal)) == sizeof(portal);
    storage.end();
    if (!saved || !storage.begin("watchsyncdev", false)) return false;
    const bool removed = !storage.isKey("hostname") || storage.remove("hostname");
    const bool finished = removed && storage.putBool("reset", false) == sizeof(bool);
    storage.end();
    return finished;
  }
};
}  // namespace watchsync
