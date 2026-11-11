#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
// Generation-scoped DNS and SNTP transport by axelaberg91.
#include "ntp_config.h"
#include <atomic>
struct tcpip_api_call_data;
struct ip_addr;

namespace watchsync {
class NtpTransport {
 public:
  bool configure(const NtpRecord *record);
  bool retryDns();
  bool unresolved() const { return unresolved_.load(); }
 private:
  struct Lookup { NtpTransport *owner = nullptr; uint32_t generation = 0; uint8_t index = 0; bool used = false; };
  Lookup lookups_[4];
  uint32_t generation_ = 0;
  char names_[2][NTP_HOST_MAX + 1] = {};
  bool resolved_[2] = {}, wanted_[2] = {};
  std::atomic<bool> unresolved_{false};
  struct Operation;
  static int8_t apply(struct tcpip_api_call_data *message);
  static void found(const char *name, const struct ip_addr *address, void *context);
  void accept(Lookup &lookup, const struct ip_addr *address);
  void resolve();
};
}  // namespace watchsync
