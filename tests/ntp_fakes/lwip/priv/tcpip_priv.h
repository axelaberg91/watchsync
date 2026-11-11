#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
#include "../apps/sntp.h"
struct tcpip_api_call_data {};
inline err_t tcpip_api_call(err_t (*call)(tcpip_api_call_data *), tcpip_api_call_data *message) { return call(message); }
