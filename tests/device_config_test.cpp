// SPDX-License-Identifier: CC-BY-NC-4.0
// Identity migration and interrupted factory reset checks by axelaberg91.
#include <cassert>
#include <cstdio>
#include "../watchsync/device_config.h"

using watchsync::DeviceConfiguration;

static void seed() {
  fakeNVS::data.clear(); fakeNVS::writesLeft = -1;
  Preferences storage;
  for (const char *name : {"watchsync", "clocksync", "watchsyncwifi", "clocksyncwifi", "otadata"}) {
    assert(storage.begin(name, false));
    assert(storage.putString("sample", "preserve-until-reset")); storage.end();
  }
}

static bool resetMarked() {
  Preferences storage; assert(storage.begin("watchsyncdev", true));
  const bool marked = storage.getBool("reset", false); storage.end(); return marked;
}

static void verifyReset() {
  for (const char *name : {"watchsync", "clocksync", "clocksyncwifi"}) assert(fakeNVS::data.at(name).empty());
  assert(fakeNVS::data.at("otadata").count("sample"));
  assert(!resetMarked());
  assert(!fakeNVS::data.at("watchsyncdev").count("hostname"));
  const auto &record = fakeNVS::data.at("watchsyncwifi").at("network");
  assert(record.size() == sizeof(watchsync::StoredWifiNetwork));
  watchsync::StoredWifiNetwork network = {};
  memcpy(&network, record.data(), record.size());
  assert(network.magic == watchsync::PORTAL_MAGIC && !network.ssid[0] && !network.psk[0]);
}

int main() {
  static_assert(sizeof(watchsync::StoredWifiNetwork) == 104, "Preserve the deployed Wi-Fi record layout");
  for (const char *name : {"watchsync", "watchsync-bedroom", "1", "a-b", "01234567890123456789012345678901"})
    assert(watchsync::validHostname(name, strlen(name)));
  for (const char *name : {"", "-bad", "bad-", "bad.local", "bad name", "Bad", "bad_name", "http://bad", "012345678901234567890123456789012"})
    assert(!watchsync::validHostname(name, strlen(name)));
  assert(!watchsync::validHostname("a\0b", 3));
  seed();
  DeviceConfiguration device; assert(device.begin());
  assert(!strcmp(device.hostname(), "watchsync") && !device.pending());
  assert(device.saveHostname("watchsync-bedroom", 17));
  assert(device.pending() && !strcmp(device.hostname(), "watchsync"));
  DeviceConfiguration reboot; assert(reboot.begin());
  assert(!strcmp(reboot.hostname(), "watchsync-bedroom") && !reboot.pending());
  assert(fakeNVS::data.at("watchsyncwifi").count("sample"));
  fakeNVS::writesLeft = 0;
  assert(!reboot.saveHostname("other", 5) && !reboot.pending());
  assert(!reboot.requestFactoryReset());
  fakeNVS::writesLeft = -1;
  assert(reboot.requestFactoryReset());
  assert(resetMarked() && fakeNVS::data.at("watchsync").count("sample"));
  const auto snapshot = fakeNVS::data;
  fakeNVS::writes = 0;
  DeviceConfiguration clean; assert(clean.begin());
  const int steps = fakeNVS::writes;
  assert(!strcmp(clean.hostname(), "watchsync")); verifyReset();
  // Power loss at every mutation leaves the marker set and retries on the next boot.
  for (int cut = 0; cut < steps; ++cut) {
    fakeNVS::data = snapshot; fakeNVS::writesLeft = cut;
    DeviceConfiguration interrupted; assert(!interrupted.begin());
    assert(resetMarked());
    fakeNVS::writesLeft = -1;
    DeviceConfiguration recovered; assert(recovered.begin());
    assert(!strcmp(recovered.hostname(), "watchsync")); verifyReset();
  }
  std::printf("PASS device settings: name validation, OTA persistence, legacy cleanup, %d interrupted-reset boundaries, OTA data preserved\n", steps);
}
