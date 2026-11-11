#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
// Deterministic SNTP and DNS transport fixture by axelaberg91.
#include <cstdint>
#include <vector>
#include <string>
#include <functional>
using err_t = int8_t;
constexpr err_t ERR_OK = 0, ERR_INPROGRESS = -5;
constexpr unsigned SNTP_MAX_SERVERS = 3, SNTP_OPMODE_POLL = 0;
#define SNTP_GET_SERVERS_FROM_DHCP 1
struct ip_addr { uint32_t value; };
using ip_addr_t = ip_addr;
inline bool IP_IS_V4(const ip_addr_t *) { return true; }
inline bool ip_addr_isany(const ip_addr_t *ip) { return !ip->value; }
inline bool ip_addr_ismulticast(const ip_addr_t *) { return false; }
inline const ip_addr_t *ip_2_ip4(const ip_addr_t *ip) { return ip; }
inline bool ip4_addr_isloopback(const ip_addr_t *) { return false; }
inline uint32_t ip4_addr_get_u32(const ip_addr_t *ip) { return ip->value; }
constexpr uint32_t IPADDR_BROADCAST = 0xffffffff;
namespace fakeNtp {
inline bool enabled = false, dhcp = true;
inline err_t dnsResult = ERR_INPROGRESS;
inline ip_addr_t servers[3] = {};
inline std::function<void()> beforeStop;
using Callback = void (*)(const char *, const ip_addr_t *, void *);
struct Pending { std::string name; Callback callback; void *context; };
inline std::vector<Pending> pending;
inline void answer(size_t index, uint32_t value) {
  auto request = pending.at(index); pending.erase(pending.begin() + index);
  ip_addr_t address{value}; request.callback(request.name.c_str(), value ? &address : nullptr, request.context);
}
}
inline void sntp_stop() { if (fakeNtp::beforeStop) fakeNtp::beforeStop(); fakeNtp::enabled = false; }
inline void sntp_setserver(unsigned index, const ip_addr_t *address) { fakeNtp::servers[index] = address ? *address : ip_addr_t{}; }
inline void sntp_setoperatingmode(unsigned) {}
inline void sntp_servermode_dhcp(int enabled) { fakeNtp::dhcp = enabled; }
inline bool sntp_enabled() { return fakeNtp::enabled; }
inline void sntp_init() { fakeNtp::enabled = true; }
