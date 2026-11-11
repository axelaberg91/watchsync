// SPDX-License-Identifier: CC-BY-NC-4.0
// Time-source persistence, recovery and validation checks by axelaberg91.
#include <cassert>
#include <cstdio>
#include "../watchsync/ntp_config.h"
#include "../watchsync/device_config.h"
using namespace watchsync;
int main() {
  for (const char *host : {"pool.ntp.org", "TIME.EXAMPLE", "router", "ntp.lan", "192.0.2.12", "10.0.0.1"})
    assert(validNtpHost(host, strlen(host)));
  for (const char *host : {"", "ntp..lan", "-ntp.lan", "ntp-.lan", "ntp.lan.", "ntp_server", "https://ntp.lan", "ntp:123", "::1", "256.0.0.1", "192.0.2", "01.2.3.4", "0.0.0.0", "127.0.0.1", "224.0.0.1", "255.255.255.255"})
    assert(!validNtpHost(host, strlen(host)));
  assert(!validNtpHost("a\0b", 3));
  std::string longest = std::string(63, 'a') + "." + std::string(63, 'b') + "." + std::string(63, 'c') + "." + std::string(61, 'd');
  assert(validNtpHost(longest.c_str(), longest.size()));
  longest += 'd'; assert(!validNtpHost(longest.c_str(), longest.size()));
  assert(!validNtpHost(std::string(64, 'a').c_str(), 64));
  NtpConfiguration initial; initial.load();
  assert(!initial.error() && !initial.record().custom);
  NtpRecord custom; custom.custom = 1; strcpy(custom.primary, "192.0.2.12");
  assert(initial.save(custom));
  NtpConfiguration reboot; reboot.load();
  assert(!reboot.error() && reboot.record().custom && !strcmp(reboot.record().first(), custom.primary) && !reboot.record().second()[0]);
  NtpRecord other = custom; strcpy(other.secondary, "ntp.example");
  fakeNVS::writesLeft = 0;
  assert(!reboot.save(other) && !reboot.record().second()[0]);
  NtpConfiguration afterFailure; afterFailure.load(); assert(!afterFailure.record().second()[0]);
  fakeNVS::writesLeft = -1;
  auto &bytes = fakeNVS::data["watchsync"]["timesource"];
  bytes[0] = 99;
  NtpConfiguration corrupt; corrupt.load(); assert(corrupt.error());
  assert(corrupt.save(other) && !corrupt.error());
  NtpConfiguration updated; updated.load(); assert(!strcmp(updated.record().second(), "ntp.example"));
  fakeNVS::data["watchsync"]["timesource"].pop_back();
  NtpConfiguration truncated; truncated.load(); assert(truncated.error());
  assert(truncated.save(NtpRecord()));
  DeviceConfiguration device; assert(device.begin()); assert(device.requestFactoryReset());
  DeviceConfiguration reset; assert(reset.begin());
  NtpConfiguration resetTime; resetTime.load(); assert(!resetTime.error() && !resetTime.record().custom);
  puts("PASS NTP settings: host validation, default migration, OTA persistence, failed write, corrupt record and factory reset");
}
