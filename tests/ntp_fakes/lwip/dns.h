#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
#include "apps/sntp.h"
inline err_t dns_gethostbyname(const char *name, ip_addr_t *address, fakeNtp::Callback callback, void *context) {
  if (fakeNtp::dnsResult != ERR_INPROGRESS) { address->value = 777; return fakeNtp::dnsResult; }
  fakeNtp::pending.push_back({name, callback, context}); return ERR_INPROGRESS;
}
