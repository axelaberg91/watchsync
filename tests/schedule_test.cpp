// SPDX-License-Identifier: CC-BY-NC-4.0
// Calendar and temporary-session checks by axelaberg91; use a POSIX host.
#include <stdio.h>
#include <stdlib.h>
#include "../watchsync/transmission_policy.h"
#include "../watchsync/time_policy.h"
#define CHECK(x) do { if(!(x)){fprintf(stderr,"Failure at line %d: %s\n",__LINE__,#x);exit(1);} }while(0)
time_t at(int year,int month,int day,int hour,int minute,int second=0) {
  struct tm value={};value.tm_year=year-1900;value.tm_mon=month-1;value.tm_mday=day;
  value.tm_hour=hour;value.tm_min=minute;value.tm_sec=second;value.tm_isdst=-1;return mktime(&value);
}
void schedules() {
  watchsync::DailySchedule schedule;CHECK(schedule.valid());CHECK(schedule.allows(at(2026,10,20,12,0)));
  schedule.enabled=true;
  CHECK(!schedule.allows(at(2026,10,19,23,59,59)));CHECK(schedule.allows(at(2026,10,20,0,0)));
  CHECK(schedule.allows(at(2026,10,20,4,59,59)));CHECK(!schedule.allows(at(2026,10,20,5,0)));
  CHECK(schedule.nextStart(at(2026,10,20,12,0))==at(2026,10,21,0,0));
  CHECK(schedule.nextStart(at(2026,10,20,23,59))==at(2026,10,21,0,0));
  schedule.startMinute=22*60;
  CHECK(schedule.allows(at(2026,10,20,23,0)));CHECK(schedule.allows(at(2026,10,21,2,0)));
  CHECK(!schedule.allows(at(2026,10,21,5,0)));CHECK(!schedule.allows(at(2026,10,21,12,0)));
  CHECK(schedule.currentWindow(at(2026,10,21,2,0)).start==at(2026,10,20,22,0));
  schedule.startMinute=0;
  auto spring=schedule.currentWindow(at(2026,3,29,4,0));CHECK(spring.end-spring.start==4*3600);
  auto autumn=schedule.currentWindow(at(2026,10,25,4,0));CHECK(autumn.end-autumn.start==6*3600);
  CHECK(schedule.nextStart(at(2026,12,31,23,59))==at(2027,1,1,0,0));
  schedule.endMinute=schedule.startMinute;CHECK(!schedule.valid());CHECK(!schedule.allows(at(2026,10,20,2,0)));
  schedule.endMinute=1440;CHECK(!schedule.valid());
  puts("PASS daily schedule: boundaries, midnight, year rollover, DST, invalid ranges");
}
void sessions() {
  watchsync::OneHourSession session;CHECK(!session.update(false,0,0));CHECK(session.update(true,1,100));
  session.beginAtBoundary(false,120,0,100);CHECK(session.pending());
  session.beginAtBoundary(true,145,0,200);CHECK(session.pending());
  session.beginAtBoundary(true,180,1000,3000);CHECK(!session.pending());
  const int64_t deadline=2000+watchsync::SESSION_DURATION_US;CHECK(session.endsUs==deadline);
  session.beginAtBoundary(true,240,1000,60003000);CHECK(session.endsUs==deadline);
  CHECK(session.update(true,1,deadline-1));CHECK(!session.update(true,1,deadline));
  CHECK(!session.update(true,1,deadline+1000000));CHECK(session.finished);
  CHECK(session.update(true,2,deadline+1000001));CHECK(session.pending());
  CHECK(!session.update(false,3,deadline+1000002));
  watchsync::TimeTrust trust;CHECK(!trust.fresh(0));trust.record(0,1577836800);
  CHECK(!trust.fresh(watchsync::MAX_HOLDOVER_US));
  for (unsigned i=4;i<100004;++i) {
    CHECK(session.update(true,i,0));session.beginAtBoundary(true,0,0,1);
    CHECK(!session.update(true,i,watchsync::SESSION_DURATION_US+1));
  }
  puts("PASS one-hour session: frame alignment, monotonic expiry, no rearm, cancellation, 100000 cycles");
}
int main() {setenv("TZ","CET-1CEST,M3.5.0/2,M10.5.0/3",1);tzset();schedules();sessions();}
