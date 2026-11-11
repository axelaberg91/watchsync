// SPDX-License-Identifier: CC-BY-NC-4.0
// Stale DNS rejection and exact configured endpoint checks by axelaberg91.
#include <cassert>
#include <cstdio>
#include "../watchsync/ntp_transport.cpp"
int main() {
  watchsync::NtpTransport transport;
  watchsync::NtpRecord defaults;
  assert(transport.configure(&defaults));
  assert(fakeNtp::pending.size() == 2 && !fakeNtp::dhcp);
  watchsync::NtpRecord local; local.custom = 1; strcpy(local.primary, "ntp.lan");
  bool fenced = false; fakeNtp::beforeStop = [&]{ fenced = true; };
  assert(transport.configure(nullptr) && fenced && !fakeNtp::enabled);
  fakeNtp::beforeStop = {};
  assert(transport.configure(&local));
  assert(fakeNtp::pending.back().name == "ntp.lan");
  fakeNtp::answer(0, 111); fakeNtp::answer(0, 222);
  assert(!fakeNtp::enabled && !fakeNtp::servers[0].value && !fakeNtp::servers[1].value);
  fakeNtp::answer(0, 333);
  assert(fakeNtp::enabled && fakeNtp::servers[0].value == 333 && !fakeNtp::servers[1].value && !fakeNtp::servers[2].value);
  assert(!transport.unresolved());
  assert(transport.configure(&defaults));
  fakeNtp::answer(0, 0); fakeNtp::answer(0, 444);
  assert(fakeNtp::enabled && fakeNtp::servers[1].value == 444 && transport.unresolved());
  assert(transport.retryDns() && fakeNtp::enabled && fakeNtp::servers[1].value == 444);
  assert(fakeNtp::pending.size() == 1 && fakeNtp::pending[0].name == "pool.ntp.org");
  assert(transport.retryDns() && fakeNtp::pending.size() == 1);
  fakeNtp::answer(0, 123); assert(!transport.unresolved());
  // Repeated changes keep pending DNS bounded and old callbacks cannot enable SNTP.
  assert(transport.configure(&defaults)); assert(transport.configure(&defaults)); assert(transport.configure(&local));
  assert(fakeNtp::pending.size() == 4 && !fakeNtp::enabled);
  while (!fakeNtp::pending.empty()) fakeNtp::answer(0, 555);
  assert(!fakeNtp::enabled);
  assert(transport.configure(&local)); fakeNtp::answer(0, 666);
  assert(fakeNtp::enabled && fakeNtp::servers[0].value == 666);
  assert(transport.configure(nullptr)); assert(!fakeNtp::enabled);
  assert(transport.retryDns() && !fakeNtp::enabled && fakeNtp::pending.empty());
  fakeNtp::dnsResult = ERR_OK;
  strcpy(local.primary, "192.0.2.12");
  assert(transport.configure(&local) && fakeNtp::enabled && !transport.unresolved());
  assert(fakeNtp::servers[0].value == 777 && !fakeNtp::servers[1].value);
  fakeNtp::dnsResult = -16;
  assert(transport.configure(&local) && !fakeNtp::enabled && transport.unresolved());
  puts("PASS NTP transport: serialized stop, obsolete DNS rejection, custom-only endpoints, DNS failure, secondary server and bounded pending lookups");
}
