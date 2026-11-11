#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
#include "apps/sntp.h"
inline err_t dns_gethostbyname(const char *name, ip_addr_t *, fakeNtp::Callback callback, void *context) {
  fakeNtp::pending.push_back({name, callback, context}); return ERR_INPROGRESS;
}
