// SPDX-License-Identifier: CC-BY-NC-4.0
// Serialized time-source changes and stale DNS rejection by axelaberg91.
#include "ntp_transport.h"
#include <lwip/apps/sntp.h>
#include <lwip/dns.h>
#include <lwip/priv/tcpip_priv.h>

namespace watchsync {
struct NtpTransport::Operation : tcpip_api_call_data {
  NtpTransport *owner;
  const NtpRecord *record;
  bool retry = false;
};

bool NtpTransport::configure(const NtpRecord *record) {
  Operation operation = {};
  operation.owner = this;
  operation.record = record;
  // Wait for the TCP/IP task: all older time callbacks finish before returning.
  return tcpip_api_call(apply, &operation) == ERR_OK;
}

bool NtpTransport::retryDns() {
  Operation operation = {};
  operation.owner = this;
  operation.retry = true;
  return tcpip_api_call(apply, &operation) == ERR_OK;
}

int8_t NtpTransport::apply(tcpip_api_call_data *message) {
  auto &operation = *static_cast<Operation *>(message);
  auto &self = *operation.owner;
  if (operation.retry) { self.resolve(); return ERR_OK; }
  sntp_stop();
  ++self.generation_;
  for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i) sntp_setserver(i, nullptr);
  self.unresolved_.store(false);
  self.resolved_[0] = self.resolved_[1] = false;
  self.wanted_[0] = self.wanted_[1] = false;
  if (!operation.record) return ERR_OK;
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
#if SNTP_GET_SERVERS_FROM_DHCP
  sntp_servermode_dhcp(0);
#endif
  for (uint8_t i = 0; i < 2; ++i) {
    const char *name = i ? operation.record->second() : operation.record->first();
    strcpy(self.names_[i], name);
    self.wanted_[i] = name[0] != 0;
  }
  self.resolve();
  return ERR_OK;
}

void NtpTransport::resolve() {
  for (uint8_t i = 0; i < 2; ++i) {
    if (!wanted_[i] || resolved_[i]) continue;
    bool pending = false;
    for (const auto &slot : lookups_)
      if (slot.used && slot.generation == generation_ && slot.index == i) pending = true;
    if (pending) continue;
    Lookup *lookup = nullptr;
    for (auto &slot : lookups_) if (!slot.used) { lookup = &slot; break; }
    if (!lookup) continue;  // Bounded pending DNS; the loop retries later.
    *lookup = {this, generation_, i, true};
    ip_addr_t address;
    const err_t result = dns_gethostbyname(names_[i], &address, found, lookup);
    if (result != ERR_INPROGRESS) accept(*lookup, result == ERR_OK ? &address : nullptr);
  }
  unresolved_.store((wanted_[0] && !resolved_[0]) || (wanted_[1] && !resolved_[1]));
}

void NtpTransport::found(const char *, const ip_addr_t *address, void *context) {
  auto &lookup = *static_cast<Lookup *>(context);
  lookup.owner->accept(lookup, address);
}

void NtpTransport::accept(Lookup &lookup, const ip_addr_t *address) {
  lookup.used = false;
  if (lookup.generation != generation_) return;
  if (address && IP_IS_V4(address) && !ip_addr_isany(address) && !ip_addr_ismulticast(address) &&
      !ip4_addr_isloopback(ip_2_ip4(address)) && ip4_addr_get_u32(ip_2_ip4(address)) != IPADDR_BROADCAST) {
    resolved_[lookup.index] = true;
    // SNTP receives numeric endpoints only, so it never retains an old DNS callback.
    sntp_setserver(lookup.index, address);
    if (!sntp_enabled()) sntp_init();
  }
  unresolved_.store((wanted_[0] && !resolved_[0]) || (wanted_[1] && !resolved_[1]));
}
}  // namespace watchsync
