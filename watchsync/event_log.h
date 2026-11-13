#pragma once
// SPDX-License-Identifier: CC-BY-NC-4.0
// Bounded device-event history by axelaberg91; callers serialize access.
#include <stddef.h>
#include <stdint.h>

namespace watchsync {
enum class EventKind : uint8_t {
  Startup, TimeSynchronized, WifiConnected, WifiDisconnected,
  RadioStarted, RadioStopped, CarrierTestStarted, CarrierTestStopped,
  RadioError, TimeInvalid, TimeZoneError, NtpConfigurationError
};
struct DeviceEvent {
  int64_t uptimeUs = 0;
  int64_t epochSeconds = 0;
  uint32_t sequence = 0;
  int32_t detail = 0;
  EventKind kind = EventKind::Startup;
};
class EventHistory {
 public:
  static constexpr size_t CAPACITY = 64;
  void record(EventKind kind, int64_t uptimeUs, int64_t epoch, int32_t detail = 0) {
    DeviceEvent &event = events_[next_];
    event = {uptimeUs, epoch, ++sequence_, detail, kind};
    next_ = (next_ + 1) % CAPACITY;
    if (count_ < CAPACITY) ++count_;
  }
  size_t copyNewest(DeviceEvent *destination, size_t limit) const {
    const size_t count = count_ < limit ? count_ : limit;
    for (size_t i = 0; i < count; ++i)
      destination[i] = events_[(next_ + CAPACITY - 1 - i) % CAPACITY];
    return count;
  }
  void timeSynchronized(int64_t uptimeUs, int64_t epoch) {
    record(EventKind::TimeSynchronized, uptimeUs, epoch);
    fresh_ = true;
  }
  void observeTime(bool fresh, int64_t uptimeUs, int64_t epoch) {
    if (fresh_ && !fresh) record(EventKind::TimeInvalid, uptimeUs, epoch);
    fresh_ = fresh;
  }
  void observeWifi(bool connected, int64_t uptimeUs, int64_t epoch) {
    if (connected != wifi_) {
      record(connected ? EventKind::WifiConnected : EventKind::WifiDisconnected, uptimeUs, epoch);
      wifi_ = connected;
    }
  }
  void observeRadio(bool active, int station, bool carrierTest, uint32_t startSequence,
                    int error, int64_t uptimeUs, int64_t epoch) {
    const bool restarted = active && active_ && (startSequence != startSequence_ || station != station_ || carrierTest != carrierTest_);
    if (active_ && (!active || restarted))
      record(carrierTest_ ? EventKind::CarrierTestStopped : EventKind::RadioStopped,
             uptimeUs, epoch, station_);
    if (active && (!active_ || restarted))
      record(carrierTest ? EventKind::CarrierTestStarted : EventKind::RadioStarted,
             uptimeUs, epoch, station);
    if (error != 0 && error != error_) record(EventKind::RadioError, uptimeUs, epoch, error);
    active_ = active; station_ = station; carrierTest_ = carrierTest;
    startSequence_ = startSequence; error_ = error;
  }
  void observeConfiguration(bool zoneError, bool ntpError, int64_t uptimeUs, int64_t epoch) {
    if (zoneError && !zoneError_) record(EventKind::TimeZoneError, uptimeUs, epoch);
    if (ntpError && !ntpError_) record(EventKind::NtpConfigurationError, uptimeUs, epoch);
    zoneError_ = zoneError; ntpError_ = ntpError;
  }
 private:
  DeviceEvent events_[CAPACITY] = {};
  size_t next_ = 0, count_ = 0;
  uint32_t sequence_ = 0, startSequence_ = 0;
  int station_ = -1, error_ = 0;
  bool active_ = false, carrierTest_ = false, wifi_ = false, fresh_ = false;
  bool zoneError_ = false, ntpError_ = false;
};
}  // namespace watchsync
