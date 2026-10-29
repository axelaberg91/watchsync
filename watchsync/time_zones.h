#pragma once

// SPDX-License-Identifier: CC-BY-NC-4.0
// Time-zone presets for watchsync by axelaberg91.

#include <stddef.h>
#include <string.h>

namespace watchsync {

struct TimeZoneDefinition {
  const char *id;
  const char *name;
  const char *rule;
};

// Persist stable IDs, rather than positions in this table.
static const TimeZoneDefinition TIME_ZONES[] = {
  {"central-europe", "Central Europe (CET/CEST)", "CET-1CEST,M3.5.0/2,M10.5.0/3"},
  {"united-kingdom", "United Kingdom (GMT/BST)", "GMT0BST,M3.5.0/1,M10.5.0/2"},
  {"japan", "Japan (JST, UTC+9)", "JST-9"},
  {"china-taiwan", "China / Taiwan (UTC+8)", "CST-8"},
  {"us-pacific", "US Pacific (PST/PDT)", "PST8PDT,M3.2.0/2,M11.1.0/2"},
  {"us-mountain", "US Mountain (MST/MDT)", "MST7MDT,M3.2.0/2,M11.1.0/2"},
  {"us-central", "US Central (CST/CDT)", "CST6CDT,M3.2.0/2,M11.1.0/2"},
  {"us-eastern", "US Eastern (EST/EDT)", "EST5EDT,M3.2.0/2,M11.1.0/2"},
  {"us-arizona", "Arizona (MST, no DST)", "MST7"},
  {"us-hawaii", "Hawaii (HST, no DST)", "HST10"},
  {"utc", "UTC (no DST)", "UTC0"}
};

constexpr size_t TIME_ZONE_COUNT = sizeof(TIME_ZONES) / sizeof(TIME_ZONES[0]);
constexpr int DEFAULT_TIME_ZONE = 0;

inline int findTimeZone(const char *id) {
  if (!id) return -1;
  for (size_t i = 0; i < TIME_ZONE_COUNT; ++i) {
    if (strcmp(id, TIME_ZONES[i].id) == 0) return (int)i;
  }
  return -1;
}

}  // namespace watchsync
