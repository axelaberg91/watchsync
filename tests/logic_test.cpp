// SPDX-License-Identifier: CC-BY-NC-4.0
// DCF77, time-policy, button and status checks for this fork by axelaberg91.
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

// Fixed Europe/Stockholm calendar fixtures. The encoder itself is compiled unchanged.
struct LocalFixture { time_t epoch; int year,month,day,dow,hour,minute,second,dst; };
static const LocalFixture localFixtures[] = {
  {1774742400,2026,3,29,0,1,0,0,0},
  {1774742460,2026,3,29,0,1,1,0,0},
  {1774745940,2026,3,29,0,1,59,0,0},
  {1774746000,2026,3,29,0,3,0,0,1},
  {1774746060,2026,3,29,0,3,1,0,1},
  {1774749540,2026,3,29,0,3,59,0,1},
  {1774749600,2026,3,29,0,4,0,0,1},
  {1792233240,2026,10,17,6,12,34,0,1},
  {1792233300,2026,10,17,6,12,35,0,1},
  {1792236840,2026,10,17,6,13,34,0,1},
  {1792274340,2026,10,17,6,23,59,0,1},
  {1792274400,2026,10,18,0,0,0,0,1},
  {1792277940,2026,10,18,0,0,59,0,1},
  {1792319640,2026,10,18,0,12,34,0,1},
  {1792319700,2026,10,18,0,12,35,0,1},
  {1792323240,2026,10,18,0,13,34,0,1},
  {1792886400,2026,10,25,0,2,0,0,1},
  {1792886460,2026,10,25,0,2,1,0,1},
  {1792889940,2026,10,25,0,2,59,0,1},
  {1792890000,2026,10,25,0,2,0,0,0},
  {1792890060,2026,10,25,0,2,1,0,0},
  {1792893540,2026,10,25,0,2,59,0,0},
  {1792893600,2026,10,25,0,3,0,0,0},
  {1793487540,2026,10,31,6,23,59,0,0},
  {1793487600,2026,11,1,0,0,0,0,0},
  {1793491140,2026,11,1,0,0,59,0,0},
  {1798757940,2026,12,31,4,23,59,0,0},
  {1798758000,2027,1,1,5,0,0,0,0},
  {1798761540,2027,1,1,5,0,59,0,0},
  {1835391540,2028,2,28,1,23,59,0,0},
  {1835391600,2028,2,29,2,0,0,0,0},
  {1835395140,2028,2,29,2,0,59,0,0},
  {1835477940,2028,2,29,2,23,59,0,0},
  {1835478000,2028,3,1,3,0,0,0,0},
  {1835481540,2028,3,1,3,0,59,0,0}
};
struct tm *fixtureLocaltime(const time_t *epoch,struct tm *out) {
  for(const auto &f:localFixtures) if(f.epoch==*epoch) {
    *out={};out->tm_year=f.year-1900;out->tm_mon=f.month-1;out->tm_mday=f.day;out->tm_wday=f.dow;
    out->tm_hour=f.hour;out->tm_min=f.minute;out->tm_sec=f.second;out->tm_isdst=f.dst;return out;
  }
  fprintf(stderr,"Missing fixture %lld\n",(long long)*epoch);exit(1);
}
#define localtime_r fixtureLocaltime
#include "../watchsync/time_policy.h"
#undef localtime_r
#define CHECK(x) do { if(!(x)){fprintf(stderr,"Failure at line %d: %s\n",__LINE__,#x);exit(1);} }while(0)
int binary(const int8_t *b,int s,int n){int v=0;for(int i=0;i<n;++i)v|=b[s+i]<<i;return v;}
int bcd(const int8_t *b,int s,int n){return binary(b,s,n<4?n:4)+(n>4?10*binary(b,s+4,n-4):0);}
struct Case { const char *name;time_t epoch;int year,month,day,dow,hour,minute,dst,a1; };
static const Case cases[] = {
  {"ordinary",1792233240,2026,10,17,6,12,35,1,0},
  {"Sunday",1792319640,2026,10,18,7,12,35,1,0},
  {"midnight to Sunday",1792274340,2026,10,18,7,0,0,1,0},
  {"month change",1793487540,2026,11,1,7,0,0,0,0},
  {"year change",1798757940,2027,1,1,5,0,0,0,0},
  {"leap day begins",1835391540,2028,2,29,2,0,0,0,0},
  {"leap day ends",1835477940,2028,3,1,3,0,0,0,0},
  {"spring announcement starts",1774742400,2026,3,29,7,1,1,0,1},
  {"spring forward",1774745940,2026,3,29,7,3,0,1,1},
  {"spring after change",1774746000,2026,3,29,7,3,1,1,0},
  {"autumn announcement starts",1792886400,2026,10,25,7,2,1,1,1},
  {"autumn backward",1792889940,2026,10,25,7,2,0,0,1},
  {"autumn after change",1792890000,2026,10,25,7,2,1,0,0}
};
void testFrames(){for(const auto &c:cases){int8_t b[60];watchsync::encodeDCF77(c.epoch,2,b);
 CHECK(bcd(b,50,8)==c.year%100);CHECK(bcd(b,45,5)==c.month);CHECK(bcd(b,36,6)==c.day);
 CHECK(binary(b,42,3)==c.dow);CHECK(bcd(b,29,6)==c.hour);CHECK(bcd(b,21,7)==c.minute);
 CHECK(b[17]==c.dst);CHECK(b[18]==1-c.dst);CHECK(b[16]==c.a1);CHECK(b[0]==0);CHECK(b[20]==1);CHECK(b[59]==2);
 CHECK(watchsync::evenParity(b,21,8)==0);CHECK(watchsync::evenParity(b,29,7)==0);CHECK(watchsync::evenParity(b,36,23)==0);
 for(int i=0;i<59;++i)CHECK(b[i]==0||b[i]==1);printf("PASS %s\n",c.name);
}}
void testTrust(){watchsync::TimeTrust t;CHECK(!t.fresh(0));CHECK(t.remainingSeconds(0)==0);
 t.record(1000,1577836800);CHECK(t.fresh(1000));CHECK(t.remainingSeconds(1000)==23*3600);
 CHECK(t.fresh(1000+watchsync::MAX_HOLDOVER_US-1));CHECK(!t.fresh(1000+watchsync::MAX_HOLDOVER_US));
 CHECK(t.remainingSeconds(1000+watchsync::MAX_HOLDOVER_US)==0);CHECK(!t.fresh(999));
 t.record(1000+watchsync::MAX_HOLDOVER_US+100,1577836800+100);CHECK(t.fresh(t.lastSyncUs));
 CHECK(t.remainingSeconds(t.lastSyncUs)==23*3600);puts("PASS time trust: startup, boundary, monotonic reset, recovery");}
void testButton(){using watchsync::ButtonAction;watchsync::ButtonGesture b;
 CHECK(b.update(false,0)==ButtonAction::None);CHECK(b.update(true,1000)==ButtonAction::None);
 CHECK(b.update(false,1100)==ButtonAction::None);CHECK(b.update(false,1449)==ButtonAction::None);
 CHECK(b.update(false,1450)==ButtonAction::Toggle);CHECK(b.update(true,2000)==ButtonAction::None);
 CHECK(b.update(true,4999)==ButtonAction::None);CHECK(b.update(true,5000)==ButtonAction::Sync);
 CHECK(b.update(true,8000)==ButtonAction::None);CHECK(b.update(false,8100)==ButtonAction::None);
 CHECK(b.update(true,8200)==ButtonAction::None);CHECK(b.update(false,8300)==ButtonAction::None);
 CHECK(b.update(false,8650)==ButtonAction::Toggle);
 CHECK(b.update(true,9000)==ButtonAction::None);CHECK(b.update(false,9050)==ButtonAction::None);
 CHECK(b.update(true,9200)==ButtonAction::None);CHECK(b.update(false,9250)==ButtonAction::TimedSession);
 CHECK(b.update(false,9700)==ButtonAction::None);
 CHECK(b.update(true,10000)==ButtonAction::None);CHECK(b.update(false,10050)==ButtonAction::None);
 CHECK(b.update(true,10200)==ButtonAction::None);CHECK(b.update(true,13200)==ButtonAction::Sync);
 CHECK(b.update(false,13300)==ButtonAction::None);CHECK(b.update(false,14000)==ButtonAction::None);
 watchsync::ButtonGesture wrap;CHECK(wrap.update(true,UINT32_MAX-1000)==ButtonAction::None);
 CHECK(wrap.update(true,1999)==ButtonAction::Sync);CHECK(wrap.update(false,2000)==ButtonAction::None);
 puts("PASS button: delayed single, double, hold after first click, release, timer wrap");}
void testStatusLight() {
 using watchsync::StatusLight;
 using watchsync::statusLight;
 CHECK(statusLight(false,true,true,true,true,true,false,0)==StatusLight::Off);
 CHECK(statusLight(true,false,false,true,false,false,true,0)==StatusLight::Stopped);
 CHECK(statusLight(true,true,true,true,true,true,false,0)==StatusLight::Green);
 CHECK(statusLight(true,true,true,false,false,false,false,0)==StatusLight::Yellow);
 CHECK(statusLight(true,true,true,true,true,false,false,0)==StatusLight::Yellow);
 CHECK(statusLight(true,true,false,true,true,true,false,0)==StatusLight::Red);
 CHECK(statusLight(true,true,false,true,false,false,false,watchsync::MAX_HOLDOVER_US)==StatusLight::Stopped);
 CHECK(statusLight(true,true,true,true,true,false,true,0)==StatusLight::Stopped);
 CHECK(statusLight(true,true,true,true,true,true,false,watchsync::SYNC_LATE_US-1)==StatusLight::Green);
 CHECK(statusLight(true,true,true,true,true,true,false,watchsync::SYNC_LATE_US)==StatusLight::Yellow);
 CHECK(statusLight(true,true,true,true,true,false,false,0,true)==StatusLight::Purple);
 CHECK(statusLight(true,false,true,true,true,false,false,0)==StatusLight::Purple);
 CHECK(statusLight(true,false,false,true,true,false,false,0)==StatusLight::Red);
 CHECK(statusLight(true,false,true,false,false,false,false,0)==StatusLight::Yellow);
 CHECK(statusLight(true,false,true,true,true,false,false,watchsync::SYNC_LATE_US)==StatusLight::Yellow);
 CHECK(statusLight(true,true,false,true,true,false,false,0,true)==StatusLight::Red);
 CHECK(statusLight(true,true,true,true,true,false,false,watchsync::SYNC_LATE_US,true)==StatusLight::Yellow);
 CHECK(statusLight(true,true,true,true,false,false,false,watchsync::MAX_HOLDOVER_US,true)==StatusLight::Stopped);
 CHECK(statusLight(true,true,false,false,false,false,false,0,false,true)==StatusLight::Blue);
 CHECK(statusLight(true,false,false,true,false,false,true,watchsync::MAX_HOLDOVER_US,false,true)==StatusLight::Blue);
 CHECK(statusLight(true,true,true,true,true,true,false,0,false,true)==StatusLight::Blue);
 CHECK(statusLight(false,true,false,false,false,false,false,0,false,true)==StatusLight::Off);
 CHECK(statusLight(true,true,false,false,false,false,false,0,false,false)==StatusLight::Red);
 puts("PASS status LED: pause, disabled, Wi-Fi, sync age, automatic stop, driver error, setup AP");
}
void testDebounce() {
 watchsync::DebouncedButton b;
 CHECK(!b.update(false,0));CHECK(!b.update(true,1));CHECK(!b.update(false,4));
 CHECK(!b.update(true,7));CHECK(!b.update(true,16));CHECK(b.update(true,17));
 CHECK(b.update(false,20));CHECK(b.update(true,23));CHECK(b.update(false,30));
 CHECK(b.update(false,39));CHECK(!b.update(false,40));
 watchsync::DebouncedButton wrap;CHECK(!wrap.update(true,UINT32_MAX-4));
 CHECK(wrap.update(true,5));
 puts("PASS button debounce: bounce, release, timer wrap");
}
int main(){testFrames();testTrust();testButton();testDebounce();testStatusLight();puts("All native C++ checks passed.");}
