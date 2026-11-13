// SPDX-License-Identifier: CC-BY-NC-4.0
#include <cassert>
#include <iostream>
#include "../watchsync/event_log.h"
using namespace watchsync;
int main() {
  EventHistory log;
  DeviceEvent events[EventHistory::CAPACITY];
  static_assert(sizeof(EventHistory) < 2300, "Keep event capture memory bounded");
  assert(log.copyNewest(events,64)==0);
  for (int i=0;i<80;++i) log.record(EventKind::Startup,1000000LL*i,0,i);
  assert(log.copyNewest(events,64)==64);
  assert(events[0].detail==79 && events[63].detail==16);
  assert(events[0].sequence==80 && events[63].sequence==17);
  assert(log.copyNewest(events,3)==3 && events[2].detail==77);
  // Client snapshots remain unchanged when producers overwrite the ring.
  log.record(EventKind::WifiConnected,80000000,0);
  assert(events[0].detail==79);
  EventHistory restarted;
  assert(restarted.copyNewest(events,64)==0);
  restarted.observeWifi(false,0,0);
  assert(restarted.copyNewest(events,64)==0);
  restarted.observeWifi(true,1000000,0);
  restarted.observeWifi(true,2000000,0);
  restarted.observeWifi(false,3000000,0);
  assert(restarted.copyNewest(events,64)==2);
  assert(events[0].kind==EventKind::WifiDisconnected && events[1].epochSeconds==0);
  EventHistory radio;
  radio.observeRadio(false,3,false,0,0,0,0);
  radio.observeRadio(true,3,false,1,0,1000000,1800000000);
  for(int i=0;i<1000;++i) radio.observeRadio(true,3,false,1,0,2000000+i,1800000001);
  assert(radio.copyNewest(events,64)==1 && events[0].kind==EventKind::RadioStarted);
  assert(events[0].detail==3 && events[0].epochSeconds==1800000000);
  radio.observeRadio(true,3,false,2,0,3000000,1800000002);
  assert(radio.copyNewest(events,64)==3);
  assert(events[0].kind==EventKind::RadioStarted && events[1].kind==EventKind::RadioStopped);
  radio.observeRadio(false,3,false,2,42,4000000,1800000003);
  radio.observeRadio(false,3,false,2,42,5000000,1800000004);
  assert(radio.copyNewest(events,64)==5);
  assert(events[0].kind==EventKind::RadioError && events[0].detail==42);
  assert(events[1].kind==EventKind::RadioStopped);
  radio.observeRadio(true,3,true,3,0,6000000,1800000005);
  radio.observeRadio(true,3,false,3,0,7000000,1800000006);
  assert(radio.copyNewest(events,64)==8);
  assert(events[0].kind==EventKind::RadioStarted && events[1].kind==EventKind::CarrierTestStopped);
  assert(events[2].kind==EventKind::CarrierTestStarted);
  EventHistory time;
  time.observeTime(false,0,0);
  time.timeSynchronized(1000000,1800000000);
  time.timeSynchronized(2000000,1800000001);
  time.observeTime(true,3000000,1800000002);
  time.observeTime(false,4000000,0);
  time.observeTime(false,5000000,0);
  assert(time.copyNewest(events,64)==3 && events[0].kind==EventKind::TimeInvalid);
  assert(events[1].kind==EventKind::TimeSynchronized && events[1].epochSeconds==1800000001);
  time.observeConfiguration(true,true,6000000,0);
  time.observeConfiguration(true,true,7000000,0);
  assert(time.copyNewest(events,64)==5);
  time.observeConfiguration(false,false,8000000,0);
  time.observeConfiguration(true,false,9000000,0);
  assert(time.copyNewest(events,64)==6);
  std::cout << "PASS event history: wrap, snapshots, restart, timestamps, actual transitions and deduplication\n";
}
