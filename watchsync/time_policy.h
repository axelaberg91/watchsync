#pragma once

// SPDX-License-Identifier: CC-BY-NC-4.0
// DCF77 time policy, button handling and status logic by axelaberg91.

#include <stdint.h>
#include <time.h>

namespace watchsync {

constexpr int64_t MAX_HOLDOVER_US = 23LL * 60 * 60 * 1000000;
constexpr uint32_t BUTTON_HOLD_MS = 3000;
constexpr uint32_t BUTTON_DOUBLE_MS = 350;
constexpr uint32_t NTP_INTERVAL_MS = 3UL * 60 * 60 * 1000;
constexpr int64_t SYNC_LATE_US = ((int64_t)NTP_INTERVAL_MS + 15LL * 60 * 1000) * 1000;

enum class StatusLight { Off, Green, Yellow, Red, Stopped, Blue, Purple };

inline StatusLight statusLight(bool enabled, bool requested, bool wifi, bool synced,
                               bool fresh, bool transmitting, bool radioError,
                               int64_t ageUs, bool scheduledIdle = false, bool setupAP = false) {
  if (!enabled) return StatusLight::Off;
  if (setupAP) return StatusLight::Blue;
  if (radioError || (synced && !fresh)) return StatusLight::Stopped;
  if (!wifi) return StatusLight::Red;
  if (!synced || ageUs >= SYNC_LATE_US) return StatusLight::Yellow;
  if (!requested || scheduledIdle) return StatusLight::Purple;
  if (!transmitting) return StatusLight::Yellow;
  return StatusLight::Green;
}

struct TimeTrust {
  bool synced = false;
  int64_t lastSyncUs = 0;
  time_t lastSyncEpoch = 0;

  void record(int64_t monotonicUs, time_t epoch) {
    synced = true;
    lastSyncUs = monotonicUs;
    lastSyncEpoch = epoch;
  }

  bool fresh(int64_t monotonicUs) const {
    return synced && monotonicUs >= lastSyncUs &&
           monotonicUs - lastSyncUs < MAX_HOLDOVER_US;
  }

  uint32_t remainingSeconds(int64_t monotonicUs) const {
    if (!fresh(monotonicUs)) return 0;
    return (uint32_t)((MAX_HOLDOVER_US - (monotonicUs - lastSyncUs)) / 1000000);
  }
};

enum class ButtonAction { None, Toggle, Sync, TimedSession };

class DebouncedButton {
 public:
  bool update(bool pressed, uint32_t nowMs) {
    if (pressed != raw_) { raw_ = pressed; changedAt_ = nowMs; }
    if ((uint32_t)(nowMs - changedAt_) >= 10) stable_ = raw_;
    return stable_;
  }
 private:
  bool raw_ = false, stable_ = false;
  uint32_t changedAt_ = 0;
};

class ButtonGesture {
 public:
  ButtonAction update(bool pressed, uint32_t nowMs) {
    ButtonAction action = ButtonAction::None;
    if (pending_ && !down_ && (uint32_t)(nowMs - releasedAt_) >= BUTTON_DOUBLE_MS) {
      pending_ = false;
      action = ButtonAction::Toggle;
    }
    if (pressed && !down_) {
      down_ = true;
      held_ = false;
      started_ = nowMs;
      second_ = pending_;
      pending_ = false;
    }
    if (pressed && !held_ && (uint32_t)(nowMs - started_) >= BUTTON_HOLD_MS) {
      held_ = true;
      second_ = pending_ = false;
      return ButtonAction::Sync;
    }
    if (!pressed && down_) {
      down_ = false;
      if (!held_) {
        if (second_) { second_ = false; return ButtonAction::TimedSession; }
        pending_ = true;
        releasedAt_ = nowMs;
      }
    }
    return action;
  }
 private:
  bool down_ = false;
  bool held_ = false;
  bool pending_ = false, second_ = false;
  uint32_t started_ = 0;
  uint32_t releasedAt_ = 0;
};

inline void putBinary(int8_t *bits, int start, int length, unsigned value) {
  for (int i = 0; i < length; ++i) bits[start + i] = (value >> i) & 1;
}

inline void putBCD(int8_t *bits, int start, int length, unsigned value) {
  putBinary(bits, start, length, ((value / 10) << 4) | (value % 10));
}

inline int8_t evenParity(const int8_t *bits, int start, int length) {
  int result = 0;
  for (int i = 0; i < length; ++i) result ^= bits[start + i];
  return result;
}

// frameMinuteUTC is the beginning of the minute being transmitted.
// Add elapsed seconds BEFORE converting to local time, including DST transitions.
inline void encodeDCF77(time_t frameMinuteUTC, int dstOverride, int8_t *bits) {
  time_t nextMinuteUTC = frameMinuteUTC + 60;
  time_t nextHourUTC = frameMinuteUTC + 3600;
  struct tm current = {}, next = {}, nextHour = {};
  localtime_r(&frameMinuteUTC, &current);
  localtime_r(&nextMinuteUTC, &next);
  localtime_r(&nextHourUTC, &nextHour);
  for (int i = 0; i < 60; ++i) bits[i] = 0;
  bits[16] = (current.tm_isdst > 0) != (nextHour.tm_isdst > 0);
  const bool summer = dstOverride == 2 ? next.tm_isdst > 0 : dstOverride == 1;
  bits[17] = summer ? 1 : 0;
  bits[18] = summer ? 0 : 1;
  bits[20] = 1;
  putBCD(bits, 21, 7, next.tm_min);
  bits[28] = evenParity(bits, 21, 7);
  putBCD(bits, 29, 6, next.tm_hour);
  bits[35] = evenParity(bits, 29, 6);
  putBCD(bits, 36, 6, next.tm_mday);
  putBinary(bits, 42, 3, next.tm_wday == 0 ? 7 : next.tm_wday);
  putBCD(bits, 45, 5, next.tm_mon + 1);
  putBCD(bits, 50, 8, next.tm_year % 100);
  bits[58] = evenParity(bits, 36, 22);
  bits[59] = 2;  // minute marker: no amplitude reduction
}

}  // namespace watchsync

