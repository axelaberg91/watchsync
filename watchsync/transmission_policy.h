#pragma once

// SPDX-License-Identifier: CC-BY-NC-4.0
// Daily transmission windows and bounded session state by axelaberg91.

#include <stdint.h>
#include <time.h>

namespace watchsync {

constexpr int64_t SESSION_DURATION_US = 60LL * 60 * 1000000;

struct OneHourSession {
  uint32_t revision = 0;
  int64_t endsUs = 0;
  bool finished = false;

  bool update(bool requested, uint32_t requestedRevision, int64_t nowUs) {
    if (revision != requestedRevision) { revision=requestedRevision; endsUs=0; finished=false; }
    if (endsUs && nowUs>=endsUs) finished=true;
    return requested && !finished;
  }

  void beginAtBoundary(bool transmitting, time_t epoch, long microseconds, int64_t nowUs) {
    if (transmitting && !finished && !endsUs && epoch%60==0)
      endsUs=nowUs+SESSION_DURATION_US-microseconds;
  }

  bool pending() const { return !finished && !endsUs; }
};

struct DailyWindow {
  time_t start = 0;
  time_t end = 0;
};

struct DailySchedule {
  bool enabled = false;
  uint16_t startMinute = 0;
  uint16_t endMinute = 5 * 60;

  bool valid() const {
    return startMinute < 1440 && endMinute < 1440 && startMinute != endMinute;
  }

  DailyWindow windowForDay(time_t now, int dayOffset) const {
    if (!valid()) return {};
    struct tm start = {}, end = {};
    localtime_r(&now, &start);
    end = start;
    start.tm_mday += dayOffset;
    end.tm_mday += dayOffset + (endMinute <= startMinute ? 1 : 0);
    start.tm_hour = startMinute / 60; start.tm_min = startMinute % 60; start.tm_sec = 0;
    end.tm_hour = endMinute / 60; end.tm_min = endMinute % 60; end.tm_sec = 0;
    // Resolve each local date independently so DST does not imply a 24-hour day.
    start.tm_isdst = end.tm_isdst = -1;
    DailyWindow result = {mktime(&start), mktime(&end)};
    return result.start >= 0 && result.end > result.start ? result : DailyWindow{};
  }

  DailyWindow currentWindow(time_t now) const {
    if (!enabled) return {};
    for (int day = -1; day <= 0; ++day) {
      const DailyWindow window = windowForDay(now, day);
      if (window.end && now >= window.start && now < window.end) return window;
    }
    return {};
  }

  bool allows(time_t now) const {
    return !enabled || currentWindow(now).end != 0;
  }

  time_t nextStart(time_t now) const {
    if (!enabled) return 0;
    time_t next = 0;
    for (int day = 0; day <= 3; ++day) {
      const DailyWindow window = windowForDay(now, day);
      if (window.end && window.start > now && (!next || window.start < next)) next = window.start;
    }
    return next;
  }
};

}  // namespace watchsync
