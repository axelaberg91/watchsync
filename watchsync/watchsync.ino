// watchsync - local radio clock station.
// Based on nisejjy by SASAKI Taroh and clocksync by Tanvach.
// Original fork contributions by axelaberg91: CC BY-NC 4.0; see LICENSE.md.
// Inherited code is not relicensed; its license status remains unresolved.
// Fork improvements (v1.4.0-v1.12.2) by axelaberg91: DCF77 reliability,
// time-sync safeguards, Atom Lite controls, Wi-Fi configuration, web time zones, OTA, device identity and factory reset.
// Radio code, networking and display use separate execution paths.
#define VERSION "1.12.2"
// The slim enclosure connects the coil to the Atom's GPIO25 pin interface.
#define PIN_RADIO 25
#define PIN_MEAS 33
#define PIN_LED -1
#define PIN_RGB_LED 27
#define PIN_BUTTON 39
#define PIN_BUZZ -1

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <esp32-hal-rgb-led.h>
#include <Preferences.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include <esp_sntp.h>
#include "time_policy.h"
#include "transmission_policy.h"
#include "time_zones.h"
#include "web_ui.h"
#include "wifi_config.h"
#include "firmware_update.h"

// Fixed medium drive for the Atom Lite antenna output; saved user values are ignored.
static constexpr gpio_drive_cap_t ANTENNA_DRIVE_STRENGTH = GPIO_DRIVE_CAP_2;


// Protocol encoders must never write to USB from the radio task.
#define LOG_PRINT(...) ((void)0)
#define LOG_PRINTLN(...) ((void)0)
#define LOG_PRINTF(...) ((void)0)

#define SN_JJY_E (0)  // JJY Fukushima Japan
#define SN_JJY_W (1)  // JJY Fukuoka Japan
#define SN_WWVB (2)   // WWVB US
#define SN_DCF77 (3)  // DCF77 Germany
#define SN_BSF (4)    // BSF Taiwan
#define SN_MSF (5)    // MSF UK
#define SN_BPC (6)    // BPC China

#define SN_DEFAULT (SN_DCF77)

int st_cycle2[] = {
  // interrupt cycle, KHz: double of station freq
  80,   // 40KHz JJY-E
  120,  // 60KHz JJY-W
  120,  // 60KHz WWVB
  155,  // 77.5KHz DCF77
  155,  // 77.5KHz BSF
  120,  // 60KHz MSF
  137   // 68.5KHz BPC
};

// enum symbols
#define SP_0 (0)
#define SP_1 (1)
#define SP_M (2)
#define SP_P0 (SP_1)  // for MSF
#define SP_P1 (3)     // for MSF
/*#define SP_M0 (3) // for HBG
#define SP_M00 (4) // for HBG
#define SP_M000 (5) // for HBG
 */
#define SP_2 (2)   // for BSF/BPC
#define SP_3 (3)   // for BSF/BPC
#define SP_M4 (4)  // for BSF/BPC
#define SP_MAX (SP_M4)
//
// bits_STATION[] => *bits60: 60 second symbol buffers, initialized with patterns
// sp_STATION[] => *secpattern: 0.1sec term pattern in one second, for each symbol
// * note: when sp_STATION[n * 10], secpattern[] is like 2-dim array secpattern[n][10].
//
// JJY & WWVB  *note: comment is the format of JJY.
int8_t bits_jjy[] = {
  // 60bit transmitted frame, of {SP_0, SP_1, SP_M}
  SP_M, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_M,  // (M), MIN10[3], 0, MIN1[4], (M)
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_M,  // 0, 0, HOUR10[2], 0, HOUR1[4], (M)
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_M,  // 0, 0, DOY100[2], DOY10[4], (M)
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_M,  // DOY1[4], 0, 0, PA1, PA2, 0, (M)
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_M,  // 0, YEAR[8], (M)
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_M   // DOW[3], LS1, LS2, 0, 0, 0, 0, (M)
};
// *note: if summer time, set bit 57/58 (WWVB) (bit 38/40 (JJY, in future))
int8_t sp_jjy[] = {
  // in (0, 1), [SP_x][amplitude_for_0.1sec_term_in_second]
  1, 1, 1, 1, 1, 1, 1, 1, 0, 0,  // SP_0
  1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  // SP_1
  1, 1, 0, 0, 0, 0, 0, 0, 0, 0   // SP_M
};
int8_t sp_wwvb[] = {
  // in (0, 1), [SP_x][amplitude_for_0.1sec_term_in_second]
  0, 0, 1, 1, 1, 1, 1, 1, 1, 1,  // SP_0
  0, 0, 0, 0, 0, 1, 1, 1, 1, 1,  // SP_1
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1   // SP_M
};

// DCF77 encoding is LSB->MSB. //HBG *note: [0] is changed depending on DCF/HBG (also min/hour).
int8_t bits_dcf[] = {
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,  // 0, reserved[9]
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_1, SP_0,  // reserved[5], 0, 0, (0, 1)(MEZ), 0
  SP_1, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,  // 1, MIN1[4], MIN10[3], P1, (1->)
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,  // HOUR1[4], HOUR10[2], P2, D1[4]
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,  // D10[2], DOW[3], M1[4], M10[1]
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_M   // Y1[4], Y10[4], P3, (M)
};
int8_t sp_dcf[] = {
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // SP_0
  0, 0, 1, 1, 1, 1, 1, 1, 1, 1,  // SP_1
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1   // SP_M
};
/*
int8_t sp_hbg[] = {
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1,   // SP_0
  0, 0, 1, 1, 1, 1, 1, 1, 1, 1,   // SP_1
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,   // SP_M
  0, 1, 0, 1, 1, 1, 1, 1, 1, 1,   // SP_M0    // 00sec
  0, 1, 0, 1, 0, 1, 1, 1, 1, 1,   // SP_M00   // 00sec at 00min
  0, 1, 0, 1, 0, 1, 0, 1, 1, 1    // SP_M000  // 00sec at 00/12 hour 00min
};
*/

// BSF: quad encoding.
int8_t bits_bsf[] = {
  SP_M4, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_M4,
  SP_1, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,  // 1, min[3], hour[2.5], P1[.5],
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_M4  // DOM[2.5], DOW[2.5], mon[2],
                                                               // year[3.5], P2[.5], 0, 0, M
};
int8_t sp_bsf[] = {
  0, 0, 1, 1, 1, 1, 1, 1, 1, 1,  // SP_0
  0, 0, 0, 0, 1, 1, 1, 1, 1, 1,  // SP_1
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1,  // SP_2
  0, 0, 0, 0, 0, 0, 1, 1, 1, 1,  // SP_3
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1   // SP_M4
};

// MSF has 4 patterns.
int8_t bits_msf[] = {
  SP_M, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,
  SP_1, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,
  SP_0, SP_0, SP_0, SP_P0, SP_P0, SP_P0, SP_P0, SP_P0, SP_P0, SP_0
};
int8_t sp_msf[] = {
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // SP_0
  0, 0, 1, 1, 1, 1, 1, 1, 1, 1,  // SP_1/SP_P0 (parity 0)
  0, 0, 0, 0, 0, 1, 1, 1, 1, 1,  // SP_M
  0, 0, 0, 1, 1, 1, 1, 1, 1, 1   // SP_P1 (parity 1)
};

// BPC: quadary and has 5 patterns.
int8_t bits_bpc[] = {
  SP_M4, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,  // (B), P1, P2, h[2], m[3], DOW[2]
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,   // P3, D[3], M[2], Y[3], P4
  SP_M4, SP_1, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,  // P1: 0, 1, 2 for 00-19, -39, -59s
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,   // P2: 0, P3: AM/PM(0/2)+par<hmDOW>
  SP_M4, SP_2, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0,  // P4: par<DMY> (0/1)
  SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0, SP_0
};
int8_t sp_bpc[] = {
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // SP_0
  0, 0, 1, 1, 1, 1, 1, 1, 1, 1,  // SP_1
  0, 0, 0, 1, 1, 1, 1, 1, 1, 1,  // SP_2
  0, 0, 0, 0, 1, 1, 1, 1, 1, 1,  // SP_3
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0   // SP_M4
};

// func for makeup patterns
void mb_jjy(void);   // JJY-E, JJY-W
void mb_wwvb(void);  // WWVB
void mb_dcf(void);   // DCF77
void mb_bsf(void);   // BSF
void mb_msf(void);   // MSF
void mb_bpc(void);   // BPC

int8_t *st_bits[] = { bits_jjy, bits_jjy, bits_jjy, bits_dcf, bits_bsf, bits_msf, bits_bpc };
int8_t *bits60;
int8_t *st_sp[] = { sp_jjy, sp_jjy, sp_wwvb, sp_dcf, sp_bsf, sp_msf, sp_bpc };
int8_t *secpattern;
void (*st_makebits[])(void) = { mb_jjy, mb_jjy, mb_wwvb, mb_dcf, mb_bsf, mb_msf, mb_bpc };
void (*makebitpattern)(void);
const char *stationNames[] = {
  "JJY_E (40 kHz)",
  "JJY_W (60 kHz)",
  "WWVB (60 kHz)",
  "DCF77 (77.5 kHz)",
  "BSF (77.5 kHz)",
  "MSF (60 kHz)",
  "BPC (68.5 kHz)"
};
const char stationCmds[] = "jkwdtmc";

struct Control {
  bool requested = true;
  watchsync::DailySchedule schedule;
  bool sessionRequested = false;
  uint32_t sessionRevision = 0;
  bool led = true;
  int station = SN_DEFAULT;
  int pin = PIN_RADIO;
  int dst = 2;
  int timeZone = watchsync::DEFAULT_TIME_ZONE;
  uint32_t timeZoneRevision = 0;
  bool changingTimeZone = true;
  bool timeZoneError = false;
  bool testCarrier = false;
  bool editingTime = false;
  bool savingSettings = false;
  uint32_t storageRevision = 0;
  bool updatingFirmware = false;
  uint32_t firmwareRevision = 0;
  bool wifiConnected = false;
  bool setupAP = false;
};

struct RadioStatus {
  bool active = false;
  bool aligning = false;
  bool sessionPending = false;
  bool sessionFinished = false;
  uint32_t sessionRevision = 0;
  int64_t sessionEndsUs = 0;
  bool timeZonePaused = false;
  bool storagePaused = false;
  uint32_t storageRevision = 0;
  bool firmwarePaused = false;
  uint32_t firmwareRevision = 0;
  int64_t heartbeatUs = 0;
  uint32_t timeZoneRevision = 0;
  uint32_t frequency = 0;
  int error = 0;
  watchsync::StatusLight light = watchsync::StatusLight::Off;
};

static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
static Control control;
static RadioStatus radioStatus;
static watchsync::TimeTrust timeTrust;
static watchsync::ButtonGesture button;
static watchsync::DebouncedButton buttonInput;
static WebServer server(80);
static watchsync::DeviceConfiguration deviceConfiguration;
static WiFiConfiguration wifiConfiguration(server, deviceConfiguration);
static bool deviceRestartPending = false;
static uint32_t deviceRestartAt = 0;
static_assert(PIN_RADIO == 25, "Review the OTA hardware marker before changing the antenna GPIO.");
static FirmwareUpdate firmwareUpdate(server, wifiConfiguration);

static Preferences prefs;
static TaskHandle_t radioTaskHandle = nullptr;
static constexpr char NTP_PRIMARY[] = "pool.ntp.org";
static constexpr char NTP_FALLBACK[] = "time.nist.gov";
static bool ntpsync = true;
static bool syncRequested = true;
static bool ntpConfigured = false;
static bool ntpKicked = false;
static bool wasConnected = false;
static bool mdnsStarted = false;
static bool mdnsAttempted = false;
static uint32_t lastNtpKick = 0, lastMdnsAttempt = 0;
static uint32_t timeEditGeneration = 0;
static const uint32_t RETRY_MS = 60000;
static String lastCmdResp;
// Only the Arduino loop changes libc's zone, after the radio task acknowledges a pause.
static int activeTimeZone = watchsync::DEFAULT_TIME_ZONE;
static bool timeZoneApplied = false;
static bool timeZoneApplyFailed = false;
static volatile uint32_t measCount = 0;
static const ledc_mode_t LEDC_MODE = LEDC_LOW_SPEED_MODE;
static const ledc_timer_t LEDC_TIMERIDX = LEDC_TIMER_0;
static const ledc_channel_t LEDC_CHANNEL_ID = LEDC_CHANNEL_0;
static const int DUTY_HIGH = 128, DUTY_LOW = 14;

// These legacy protocol working variables are exclusively owned by radioTask.
static struct tm nowtm;
static int currentStation = SN_DEFAULT;
static int dstOverride = 2;
static time_t radioFrameMinute = 0;
static uint64_t wwvb_bits = 0;

void snapshot(Control &c, watchsync::TimeTrust &t, RadioStatus &r) {
  portENTER_CRITICAL(&stateMux);
  c = control; t = timeTrust; r = radioStatus;
  portEXIT_CRITICAL(&stateMux);
}

void publishRadio(const RadioStatus &r) {
  portENTER_CRITICAL(&stateMux);
  radioStatus = r;
  portEXIT_CRITICAL(&stateMux);
}

void updateControl(Control next) {
  const bool pending = !timeZoneApplied || next.timeZone != activeTimeZone;
  portENTER_CRITICAL(&stateMux);
  next.timeZoneRevision = control.timeZoneRevision + (pending ? 1U : 0U);
  next.changingTimeZone = pending;
  next.timeZoneError = false;
  control = next;
  portEXIT_CRITICAL(&stateMux);
  timeZoneApplyFailed = false;
}

bool applyTimeZone(int index) {
  if (index < 0 || (size_t)index >= watchsync::TIME_ZONE_COUNT) return false;
  if (setenv("TZ", watchsync::TIME_ZONES[index].rule, 1) != 0) return false;
  tzset();
  activeTimeZone = index;
  timeZoneApplied = true;
  return true;
}

void serviceTimeZone() {
  if (timeZoneApplyFailed) return;
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  if (!c.changingTimeZone || !r.timeZonePaused || r.timeZoneRevision != c.timeZoneRevision) return;
  // A matching acknowledgement guarantees that no encoder is reading the old zone.
  if (!applyTimeZone(c.timeZone)) {
    timeZoneApplyFailed = true;
    portENTER_CRITICAL(&stateMux);
    control.timeZoneError = true;
    portEXIT_CRITICAL(&stateMux);
    return;  // Fail closed; saving settings again retries, without changing time trust.
  }
  portENTER_CRITICAL(&stateMux);
  control.changingTimeZone = false;
  portEXIT_CRITICAL(&stateMux);
}

void onTimeSync(struct timeval *tv) {
  // A readable system clock is NOT proof of a new NTP reply.
  if (!tv || tv->tv_sec < 1577836800) return;
  portENTER_CRITICAL(&stateMux);
  const uint32_t generation = timeEditGeneration;
  const bool editing = control.editingTime;
  portEXIT_CRITICAL(&stateMux);
  if (editing) return;
  struct timeval wall;
  gettimeofday(&wall, nullptr);
  const int64_t difference = ((int64_t)wall.tv_sec - tv->tv_sec) * 1000000 + wall.tv_usec - tv->tv_usec;
  if (llabs(difference) > 50000) return;
  const int64_t us = esp_timer_get_time();
  portENTER_CRITICAL(&stateMux);
  if (!control.editingTime && generation == timeEditGeneration) timeTrust.record(us, tv->tv_sec);
  portEXIT_CRITICAL(&stateMux);
}

void mbc_wwvbjjy(void);
void mb_jjy(void);
void mb_wwvb(void);
void mb_bsf(void);
void mb_msf(void);
void mb_bpc(void);
void binarize(int v, int pos, int len);
void bcdize(int v, int pos, int len);
void rbinarize(int v, int pos, int len);
void rbcdize(int v, int pos, int len);
void quadize(int v, int pos, int len);
int parity(int pos, int len);
int qparity(int pos, int len);
static uint64_t to_padded5_bcd(int n);
static int is_leap_year_full(int year);
void prepareWWVBMinuteBits(time_t minuteStartUTC);
int isUSDSTPending2h(void);
int getDSTFlag(void);

void mb_dcf(void) {
  watchsync::encodeDCF77(radioFrameMinute, dstOverride, bits60);
}

void mbc_wwvbjjy(void)
{
  binarize(nowtm.tm_min / 10, 1, 3);
  binarize(nowtm.tm_min % 10, 5, 4);
  binarize(nowtm.tm_hour / 10, 12, 2);
  binarize(nowtm.tm_hour % 10, 15, 4);
  int y100 = nowtm.tm_yday / 100;
  int y1 = (nowtm.tm_yday - y100 * 100);
  int y10 = y1 / 10;
  y1 = y1 % 10;
  binarize(y100, 22, 2);
  binarize(y10, 25, 4);
  binarize(y1, 30, 4);

  return;
}

void mb_jjy(void)
{
  LOG_PRINT("encode JJY format - ");
  mbc_wwvbjjy();
  bits60[36] = parity(12, 7);
  bits60[37] = parity(1, 8);

  binarize((nowtm.tm_year - 100) / 10, 41, 4);
  binarize(nowtm.tm_year % 10, 45, 4);
  binarize(nowtm.tm_wday, 50, 3);

  return;
}

void mb_wwvb(void) {
  LOG_PRINT("encode WWVB format - ");

  time_t nowtime;
  time(&nowtime);
  struct tm *utm = gmtime(&nowtime);
  struct tm ut = *utm;

  binarize(ut.tm_min / 10, 1, 3);
  binarize(ut.tm_min % 10, 5, 4);

  binarize(ut.tm_hour / 10, 12, 2);
  binarize(ut.tm_hour % 10, 15, 4);

  int yday1 = ut.tm_yday + 1;
  binarize((yday1 / 100) % 10, 22, 2);
  binarize((yday1 / 10) % 10, 25, 4);
  binarize(yday1 % 10, 30, 4);

  binarize((ut.tm_year) / 10, 45, 4);
  binarize((ut.tm_year) % 10, 50, 4);

  bits60[57] = nowtm.tm_isdst ? SP_1 : SP_0;
  bits60[58] = isUSDSTPending2h();
  return;
}

void mb_bsf(void)
{
  LOG_PRINT("encode BSF format - ");
  quadize(nowtm.tm_min, 41, 3);
  quadize(nowtm.tm_hour * 2, 44, 3);
  bits60[46] |= qparity(41, 6);
  quadize(nowtm.tm_mday * 2, 47, 3);
  bits60[49] |= nowtm.tm_wday / 4;
  quadize(nowtm.tm_wday % 4, 50, 1);
  quadize(nowtm.tm_mon + 1, 51, 2);
  quadize((nowtm.tm_year - 100) * 2, 53, 4);
  bits60[56] |= qparity(47, 10);
  return;
}

void mb_msf(void) {
  LOG_PRINT("encode MSF format - ");
  bcdize(nowtm.tm_year - 100, 17, 8);
  bcdize(nowtm.tm_mon + 1, 25, 5);
  bcdize(nowtm.tm_mday, 30, 6);
  binarize(nowtm.tm_wday, 36, 3);
  bcdize(nowtm.tm_hour, 39, 6);
  bcdize(nowtm.tm_min, 45, 7);
  bits60[54] = parity(17, 8) * 2 + 1;
  bits60[55] = parity(25, 11) * 2 + 1;
  bits60[56] = parity(36, 3) * 2 + 1;
  bits60[57] = parity(39, 13) * 2 + 1;

  bits60[58] = getDSTFlag() ? SP_P1 : SP_1;

  return;
}

void mb_bpc(void) {
  LOG_PRINT("encode BPC format - ");
  quadize(nowtm.tm_hour % 12, 3, 2);
  quadize(nowtm.tm_min, 5, 3);
  quadize(nowtm.tm_wday, 8, 2);
  bits60[10] = (nowtm.tm_hour / 12) * 2 + qparity(3, 7);
  quadize(nowtm.tm_mday, 11, 3);
  quadize(nowtm.tm_mon + 1, 14, 2);
  quadize(nowtm.tm_year - 100, 16, 3);
  bits60[19] = qparity(11, 8);
  for (int i = 2; i < 20; i++) {
    bits60[20 + i] = bits60[i];
    bits60[40 + i] = bits60[i];
  }
  return;
}

void binarize(int v, int pos, int len) {
  for (pos = pos + len - 1; 0 < len; pos--, len--) {
    bits60[pos] = (uint8_t)(v & 1);
    v >>= 1;
  }
  return;
}

void bcdize(int v, int pos, int len) {
  int l;

  pos = pos + len - 1;
  while (0 < len) {
    if (4 <= len) {
      l = 4;
    } else {
      l = len;
    }
    binarize(v % 10, pos - l + 1, l);
    v = v / 10;
    pos = pos - l;
    len = len - l;
  }
  return;
}

void rbinarize(int v, int pos, int len) {
  for (; 0 < len; pos++, len--) {
    bits60[pos] = (uint8_t)(v & 1);
    v >>= 1;
  }
  return;
}

void rbcdize(int v, int pos, int len) {
  int l;

  LOG_PRINTF("\nrbcd %d[pos %d, len%d]=", v, pos, len);
  while (0 < len) {
    if (4 <= len) {
      l = 4;
    } else {
      l = len;
    }
    rbinarize(v % 10, pos, l);
    v = v / 10;
    pos = pos + l;
    len = len - l;
  }
  return;
}

void quadize(int v, int pos, int len) {
  for (pos = pos + len - 1; 0 < len; pos--, len--) {
    bits60[pos] = (uint8_t)(v & 3);
    v >>= 2;
  }
  return;
}

int parity(int pos, int len) {
  int s = 0;

  for (; 0 < len; pos++, len--) {
    s += bits60[pos];
  }
  return (s % 2);
}

int qparity(int pos, int len) {
  int s = 0;

  for (; 0 < len; pos++, len--) {
    s += (bits60[pos] & 1) + ((bits60[pos] & 2) >> 1);
  }
  return (s % 2);
}

static uint64_t to_padded5_bcd(int n) {
  return (uint64_t)(((n / 100) % 10) << 10) | (uint64_t)(((n / 10) % 10) << 5) | (uint64_t)(n % 10);
}

static int is_leap_year_full(int year) {
  return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
}

void prepareWWVBMinuteBits(time_t minuteStartUTC) {
  struct tm ut;
  gmtime_r(&minuteStartUTC, &ut);

  uint64_t bits = 0ULL;
  bits |= to_padded5_bcd(ut.tm_min) << (59 - 8);
  bits |= to_padded5_bcd(ut.tm_hour) << (59 - 18);
  bits |= to_padded5_bcd(ut.tm_yday + 1) << (59 - 33);
  bits |= to_padded5_bcd(ut.tm_year % 100) << (59 - 53);
  bits |= (uint64_t)(is_leap_year_full(ut.tm_year + 1900) ? 1 : 0) << (59 - 55);

  struct tm lt_now, lt_tom;
  time_t t_now_local = minuteStartUTC;
  localtime_r(&t_now_local, &lt_now);
  time_t t_tom_local = minuteStartUTC + 86400;
  localtime_r(&t_tom_local, &lt_tom);
  bits |= (uint64_t)(lt_tom.tm_isdst ? 1 : 0) << (59 - 57);
  bits |= (uint64_t)(lt_now.tm_isdst ? 1 : 0) << (59 - 58);

  wwvb_bits = bits;
}

int isUSDSTPending2h(void) {

  struct tm nowLocal = nowtm;
  time_t nowt = mktime(&nowLocal);
  int year = nowLocal.tm_year;

  auto firstSunday = [](int year1900, int mon) -> int {
    struct tm t = {};
    t.tm_year = year1900;
    t.tm_mon = mon;
    t.tm_mday = 1;
    t.tm_hour = 0;
    t.tm_isdst = -1;
    mktime(&t);
    int dow = t.tm_wday;
    return 1 + ((7 - dow) % 7);
  };

  auto secondSunday = [&](int year1900, int mon) -> int {
    int d1 = firstSunday(year1900, mon);
    return d1 + 7;
  };

  struct tm spring = {};
  spring.tm_year = year;
  spring.tm_mon = 2;
  spring.tm_mday = secondSunday(year, 2);
  spring.tm_hour = 2;
  spring.tm_isdst = -1;
  time_t tSpring = mktime(&spring);

  struct tm fall = {};
  fall.tm_year = year;
  fall.tm_mon = 10;
  fall.tm_mday = firstSunday(year, 10);
  fall.tm_hour = 2;
  fall.tm_isdst = -1;
  time_t tFall = mktime(&fall);

  time_t tx;
  if (nowLocal.tm_isdst > 0) {
    if (nowt < tFall) {
      tx = tFall;
    } else {

      fall.tm_year = year + 1;
      fall.tm_mday = firstSunday(year + 1, 10);
      tx = mktime(&fall);
    }
  } else {
    if (nowt < tSpring) {
      tx = tSpring;
    } else {

      spring.tm_year = year + 1;
      spring.tm_mday = secondSunday(year + 1, 2);
      tx = mktime(&spring);
    }
  }

  double dt = difftime(tx, nowt);
  return (dt >= 0 && dt <= 7200) ? 1 : 0;
}

int getDSTFlag(void) {
  if (dstOverride == 2) {
    return (nowtm.tm_isdst > 0) ? 1 : 0;
  }
  return dstOverride ? 1 : 0;
}

void makeFrame(const Control &c, time_t minute) {
  currentStation = c.station;
  dstOverride = c.dst;
  radioFrameMinute = minute;
  localtime_r(&minute, &nowtm);
  bits60 = st_bits[c.station];
  secpattern = st_sp[c.station];
  if (c.station == SN_WWVB) prepareWWVBMinuteBits(minute);
  else st_makebits[c.station]();
}

void indicator(watchsync::StatusLight light) {
  uint32_t color = 0;
  switch (light) {
    case watchsync::StatusLight::Green: color = 0x00ff00; break;
    case watchsync::StatusLight::Yellow: color = 0xffff00; break;
    case watchsync::StatusLight::Red: color = 0xff0000; break;
    case watchsync::StatusLight::Blue: color = 0x0000ff; break;
    case watchsync::StatusLight::Purple: color = 0x800080; break;
    case watchsync::StatusLight::Stopped:
      color = (esp_timer_get_time() / 500000) % 2 ? 0xff0000 : 0;
      break;
    default: break;
  }
  static uint32_t previous = UINT32_MAX;
  if (previous == color) return;
  previous = color;
#if PIN_LED >= 0
  digitalWrite(PIN_LED, color ? HIGH : LOW);
#elif PIN_RGB_LED >= 0
  // Match the low brightness used for the Atom's single SK6812 status pixel.
  const uint8_t red = ((color >> 16) & 255) * 20 / 255;
  const uint8_t green = ((color >> 8) & 255) * 20 / 255;
  const uint8_t blue = (color & 255) * 20 / 255;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  rgbLedWrite(PIN_RGB_LED, red, green, blue);
#else
  neopixelWrite(PIN_RGB_LED, red, green, blue);
#endif
#endif
}

void stopCarrier(int pin) {
  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_ID, 0);
  ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_ID);
  pinMode(pin, INPUT);
}

esp_err_t configureCarrier(const Control &c) {
  ledc_timer_config_t timer = {};
  timer.speed_mode = LEDC_MODE;
  timer.timer_num = LEDC_TIMERIDX;
  timer.duty_resolution = LEDC_TIMER_8_BIT;
  timer.freq_hz = st_cycle2[c.station] * 500;
  timer.clk_cfg = LEDC_USE_APB_CLK;
  esp_err_t result = ledc_timer_config(&timer);
  if (result != ESP_OK) return result;
  ledc_channel_config_t channel = {};
  channel.gpio_num = c.pin;
  channel.speed_mode = LEDC_MODE;
  channel.channel = LEDC_CHANNEL_ID;
  channel.intr_type = LEDC_INTR_DISABLE;
  channel.timer_sel = LEDC_TIMERIDX;
  channel.duty = 0;  // configuring hardware must not bypass the time guard
  channel.hpoint = 0;
  result = ledc_channel_config(&channel);
  if (result == ESP_OK) {
    result = gpio_set_drive_capability((gpio_num_t)c.pin, ANTENNA_DRIVE_STRENGTH);
  }
  return result;
}

void radioTask(void *) {
  // The carrier runs in hardware; reset the device if this task stops making progress.
  const esp_err_t watchdog = esp_task_wdt_add(nullptr);
  if (watchdog != ESP_OK) {
    RadioStatus fault; fault.error=watchdog; publishRadio(fault);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));  // Fail closed if supervision is unavailable.
  }
  Control configured, previousControl;
  bool controlKnown = false;
  bool hardwareReady = false, eligibleBefore = false, attached = false;
  time_t waitUntil = 0, preparedMinute = (time_t)-1;
  int64_t lastSlot = -1, previousOffset = 0;
  bool offsetKnown = false;
  time_t scheduleMinute = (time_t)-1;
  uint32_t scheduleZoneRevision = UINT32_MAX;
  watchsync::DailySchedule cachedSchedule;
  bool scheduleOpen = true, previousScheduleOpen = true;
  watchsync::OneHourSession session;
  uint32_t lastHardwareAttempt = 0;
  int previousDuty = -1;
  RadioStatus state;

  for (;;) {
    Control c; watchsync::TimeTrust trust; RadioStatus ignored;
    snapshot(c, trust, ignored);
    const int64_t monotonic = esp_timer_get_time();
    struct timeval tv; gettimeofday(&tv, nullptr);
    const int64_t wallUs = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    const time_t minute = tv.tv_sec - tv.tv_sec % 60;
    const bool fresh = trust.fresh(monotonic);
    const bool wasFresh = offsetKnown;
    const bool calendarReady = fresh && !c.editingTime && !c.changingTimeZone && !c.savingSettings && !c.updatingFirmware;
    const bool scheduleChanged = c.schedule.enabled != cachedSchedule.enabled ||
        c.schedule.startMinute != cachedSchedule.startMinute || c.schedule.endMinute != cachedSchedule.endMinute;
    if (calendarReady && (minute != scheduleMinute || scheduleChanged || c.timeZoneRevision != scheduleZoneRevision)) {
      scheduleOpen = c.schedule.allows(tv.tv_sec);
      scheduleMinute = minute; cachedSchedule = c.schedule; scheduleZoneRevision = c.timeZoneRevision;
    } else if (!calendarReady) { scheduleOpen = !c.schedule.enabled; scheduleMinute = (time_t)-1; }
    const bool sessionWanted = session.update(c.sessionRequested,c.sessionRevision,monotonic);
    const bool eligible = calendarReady && ((c.requested && scheduleOpen) || sessionWanted);
    const bool scheduledStart = eligible && !eligibleBefore && !previousScheduleOpen && scheduleOpen && wasFresh;
    previousScheduleOpen = scheduleOpen;
    const int64_t offset = wallUs - monotonic;
    const bool clockStep = offsetKnown && llabs(offset - previousOffset) > 50000;
    previousOffset = offset;
    offsetKnown = fresh;

    const bool changed = !controlKnown || c.pin != previousControl.pin || c.station != previousControl.station ||
                         c.dst != previousControl.dst ||
                         c.timeZoneRevision != previousControl.timeZoneRevision ||
                         c.schedule.enabled != previousControl.schedule.enabled ||
                         c.schedule.startMinute != previousControl.schedule.startMinute ||
                         c.schedule.endMinute != previousControl.schedule.endMinute;
    previousControl = c;
    controlKnown = true;
    if (changed || (eligible && !eligibleBefore) || (fresh && clockStep)) {
      if (attached) stopCarrier(configured.pin);
      attached = false;
      hardwareReady = false;
      state.active = false;
      // An already trusted schedule can open on this boundary; other starts align to the next one.
      waitUntil = scheduledStart && !changed && !clockStep && tv.tv_sec % 60 == 0 && tv.tv_usec < 50000 ? minute : minute + 60;
      preparedMinute = (time_t)-1;
      previousDuty = -1;
      lastSlot = -1;
      state.error = 0;
    }
    eligibleBefore = eligible;

    if (!eligible) {
      if (attached) stopCarrier(configured.pin);
      attached = false;
      state.active = false;
      state.aligning = false;
    } else if (tv.tv_sec < waitUntil) {
      state.active = false;
      state.aligning = true;
    } else {
      state.aligning = false;
      if (!hardwareReady && (state.error == 0 || millis() - lastHardwareAttempt >= 1000)) {
        lastHardwareAttempt = millis();
        configured = c;
        state.error = configureCarrier(c);
        hardwareReady = state.error == ESP_OK;
        attached = hardwareReady;
        state.frequency = hardwareReady ? ledc_get_freq(LEDC_MODE, LEDC_TIMERIDX) : 0;
        if (!hardwareReady) pinMode(c.pin, INPUT);
      }
      if (hardwareReady) {
        if (!attached) {
          state.error = configureCarrier(c);
          attached = state.error == ESP_OK;
          previousDuty = -1;
        }
        if (attached) {
          if (preparedMinute != minute) {
            makeFrame(c, minute);
            preparedMinute = minute;
          }
          const int64_t slot = (int64_t)tv.tv_sec * 10 + tv.tv_usec / 100000;
          if (slot != lastSlot || c.testCarrier || previousDuty < 0) {
            const int second = tv.tv_sec % 60;
            const int tenth = tv.tv_usec / 100000;
            int high;
            if (c.testCarrier) high = 1;
            else if (c.station == SN_WWVB) {
              const bool marker = second == 0 || second % 10 == 9;
              const int bit = (wwvb_bits >> (59 - second)) & 1ULL;
              high = tenth >= (marker ? 8 : (bit ? 5 : 2));
            } else high = secpattern[bits60[second] * 10 + tenth] != 0;
            const int duty = high ? DUTY_HIGH : DUTY_LOW;
            if (duty != previousDuty) {
              state.error = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_ID, duty);
              if (state.error == ESP_OK) state.error = ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_ID);
              previousDuty = state.error == ESP_OK ? duty : -1;
            }
            lastSlot = slot;
          }
          state.active = state.error == ESP_OK;
          if (!state.active) {
            stopCarrier(c.pin);
            attached = hardwareReady = false;
          }
        }
      }
    }
    // Publish the acknowledgement only after stopping the carrier and skipping frame work.
    state.timeZonePaused = c.changingTimeZone;
    state.timeZoneRevision = c.timeZoneRevision;
    state.storagePaused = c.savingSettings;
    state.storageRevision = c.storageRevision;
    state.firmwarePaused = c.updatingFirmware;
    state.firmwareRevision = c.firmwareRevision;
    state.heartbeatUs = monotonic;
    // A one-hour pass starts counting only once its first frame actually begins.
    session.beginAtBoundary(state.active && sessionWanted,tv.tv_sec,tv.tv_usec,monotonic);
    state.sessionRevision = c.sessionRevision;
    state.sessionEndsUs = c.sessionRequested ? session.endsUs : 0;
    state.sessionPending = sessionWanted && session.pending();
    state.sessionFinished = c.sessionRequested && session.finished;
    state.light = watchsync::statusLight(c.led && (PIN_LED >= 0 || PIN_RGB_LED >= 0), c.requested || sessionWanted, c.wifiConnected, trust.synced,
        fresh, state.active, state.error != 0 || c.timeZoneError, monotonic - trust.lastSyncUs,
        c.requested && !scheduleOpen && !sessionWanted, c.setupAP);
    if (c.updatingFirmware && c.led && !c.setupAP) state.light = watchsync::StatusLight::Yellow;
    indicator(state.light);
    publishRadio(state);
    esp_task_wdt_reset();
    // Wall-clock sampling avoids accumulated tick drift and catches NTP corrections.
    // No HTTP, flash writes, logging or network operations run in this task.
    vTaskDelay(1);
  }
}

bool safePin(int p) {
  const int pins[] = {19,21,22,23,25,26,32,33};
  for (int n : pins) if (p == n) return true;
  return false;
}

bool pauseForFirmware() {
  if (!radioTaskHandle) return false;
  portENTER_CRITICAL(&stateMux);
  control.updatingFirmware = true;
  const uint32_t revision = ++control.firmwareRevision;
  portEXIT_CRITICAL(&stateMux);
  const uint32_t started = millis();
  while (uint32_t(millis() - started) < 250) {
    portENTER_CRITICAL(&stateMux);
    const bool paused = radioStatus.firmwarePaused && radioStatus.firmwareRevision == revision;
    portEXIT_CRITICAL(&stateMux);
    if (paused) return true;
    vTaskDelay(1);
  }
  portENTER_CRITICAL(&stateMux); control.updatingFirmware = false; portEXIT_CRITICAL(&stateMux);
  return false;
}

void resumeAfterFirmware() {
  portENTER_CRITICAL(&stateMux); control.updatingFirmware = false; portEXIT_CRITICAL(&stateMux);
}

bool firmwareStartupHealthy() {
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  const int64_t age = esp_timer_get_time() - r.heartbeatUs;
  // Offline setup is a healthy boot; internet time and Wi-Fi are not prerequisites.
  return radioTaskHandle && r.heartbeatUs > 0 && age >= 0 && age < 1000000 && !r.error && timeZoneApplied && !timeZoneApplyFailed;
}


bool pauseForStorage() {
  if (!radioTaskHandle) return true;
  portENTER_CRITICAL(&stateMux);
  control.savingSettings = true;
  const uint32_t revision = ++control.storageRevision;
  portEXIT_CRITICAL(&stateMux);
  const uint32_t started=millis();
  while ((uint32_t)(millis()-started)<250) {
    portENTER_CRITICAL(&stateMux);
    const bool paused=radioStatus.storagePaused && radioStatus.storageRevision==revision;
    portEXIT_CRITICAL(&stateMux);
    if (paused) return true;
    vTaskDelay(1);
  }
  portENTER_CRITICAL(&stateMux); control.savingSettings=false; portEXIT_CRITICAL(&stateMux);
  return false;
}

void resumeAfterStorage() {
  portENTER_CRITICAL(&stateMux); control.savingSettings=false; portEXIT_CRITICAL(&stateMux);
}

bool saveSettings() {
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  if (!pauseForStorage()) return false;
  if (!prefs.begin("watchsync", false)) { resumeAfterStorage(); return false; }
  bool saved = prefs.putInt("tx", c.requested) == sizeof(int32_t);
  const uint8_t scheduleRecord[5] = {uint8_t(c.schedule.enabled), uint8_t(c.schedule.startMinute),
      uint8_t(c.schedule.startMinute>>8), uint8_t(c.schedule.endMinute), uint8_t(c.schedule.endMinute>>8)};
  saved = prefs.putBytes("schedule", scheduleRecord, sizeof(scheduleRecord)) == sizeof(scheduleRecord) && saved;
  const bool pinSaved = prefs.putInt("pin", c.pin) == sizeof(int32_t);
  saved = pinSaved && saved;
  if (pinSaved) saved = prefs.putInt("pindefault", PIN_RADIO) == sizeof(int32_t) && saved;
  saved = prefs.putInt("dst", c.dst) == sizeof(int32_t) && saved;
  saved = prefs.putInt("led", c.led) == sizeof(int32_t) && saved;
  saved = prefs.putInt("ntp", ntpsync) == sizeof(int32_t) && saved;
  saved = prefs.putString("tzid", watchsync::TIME_ZONES[c.timeZone].id) > 0 && saved;
  // Publish the new namespace after the remaining settings have been written.
  saved = prefs.putInt("station", c.station) == sizeof(int32_t) && saved;
  prefs.end();
  resumeAfterStorage();
  return saved;
}

void loadSettings() {
  bool opened = prefs.begin("watchsync", true);
  if (opened && !prefs.isKey("station")) { prefs.end(); opened = false; }
  // Read the previous firmware's namespace only when no new settings exist.
  // This literal is a compatibility key, not a device or project name.
  if (!opened && !prefs.begin("clocksync", true)) return;
  const int station = prefs.getInt("station", SN_DEFAULT);
  const int pin = prefs.getInt("pin", PIN_RADIO);
  // Follow the selected hardware profile only when the saved pin was its prior default.
  // Explicit custom selections survive, including a GPIO32 choice in the GPIO25 build.
  const int previousDefault = prefs.getInt("pindefault", prefs.getUInt("pinlayout",0) ? 25 : 32);
  const bool migratePin = previousDefault != PIN_RADIO && pin == previousDefault;
  control.station = station >= 0 && station <= SN_BPC ? station : SN_DEFAULT;
  control.pin = migratePin ? PIN_RADIO : (safePin(pin) ? pin : PIN_RADIO);
  control.requested = prefs.getInt("tx", 1) != 0;
  if (prefs.isKey("schedule")) {
    uint8_t record[5] = {};
    const bool loaded = prefs.getBytesLength("schedule")==sizeof(record) && prefs.getBytes("schedule",record,sizeof(record))==sizeof(record);
    control.schedule.enabled=record[0]!=0;
    control.schedule.startMinute=uint16_t(record[1]) | uint16_t(record[2])<<8;
    control.schedule.endMinute=uint16_t(record[3]) | uint16_t(record[4])<<8;
    if (!loaded || record[0]>1 || !control.schedule.valid()) {
      control.schedule=watchsync::DailySchedule(); control.requested=false;
      Serial.println("Saved schedule is invalid. Automatic transmission is paused.");
    }
  }
  control.led = prefs.getInt("led", 1) != 0;
  control.dst = constrain(prefs.getInt("dst", 2), 0, 2);
  const String timeZoneId = prefs.getString("tzid", watchsync::TIME_ZONES[watchsync::DEFAULT_TIME_ZONE].id);
  const int zone = watchsync::findTimeZone(timeZoneId.c_str());
  control.timeZone = zone >= 0 ? zone : watchsync::DEFAULT_TIME_ZONE;
  ntpsync = prefs.getInt("ntp", 1) != 0;
  prefs.end();
  if (migratePin && !saveSettings()) Serial.println("GPIO profile applied but could not be saved. Check settings after restart.");
  // Time trust deliberately never persists across a reboot.
}

void requestSync() {
  ntpsync = true;
  syncRequested = true;
}

bool sessionRequestedNow(const Control &c, const RadioStatus &r, int64_t us) {
  return c.sessionRequested && (c.sessionRevision != r.sessionRevision ||
      (!r.sessionFinished && (!r.sessionEndsUs || us < r.sessionEndsUs)));
}

void setTimedSession(bool enabled) {
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  if (enabled && sessionRequestedNow(c,r,esp_timer_get_time())) return;
  c.sessionRequested = enabled;
  ++c.sessionRevision;
  updateControl(c);
  if (enabled && !t.fresh(esp_timer_get_time())) requestSync();
  // A temporary pass never changes the saved pause state or survives a reboot.
}

void serviceSession() {
  portENTER_CRITICAL(&stateMux);
  if (control.sessionRequested && radioStatus.sessionRevision == control.sessionRevision && radioStatus.sessionFinished)
    control.sessionRequested = false;
  portEXIT_CRITICAL(&stateMux);
}

void toggleTransmission() {
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  if (sessionRequestedNow(c,r,esp_timer_get_time())) {
    c.sessionRequested = false; ++c.sessionRevision; c.requested = false;
  } else c.requested = !c.requested;
  updateControl(c);
  saveSettings();
}

void serviceNetwork() {
  wifiConfiguration.service();
  const bool connected = WiFi.status() == WL_CONNECTED;
  const uint32_t ms = millis();
  portENTER_CRITICAL(&stateMux);
  control.wifiConnected = connected;
  control.setupAP = wifiConfiguration.portalActive();
  portEXIT_CRITICAL(&stateMux);
  if (connected && !mdnsStarted && (!mdnsAttempted || ms - lastMdnsAttempt >= RETRY_MS)) {
    mdnsAttempted = true;
    lastMdnsAttempt = ms;
    mdnsStarted = MDNS.begin(deviceConfiguration.hostname());
    if (mdnsStarted) MDNS.addService("http", "tcp", 80);
  } else if (!connected && mdnsStarted) {
    MDNS.end(); mdnsStarted = false; mdnsAttempted = false;
  }
  if (connected && ntpsync && (!ntpConfigured || !wasConnected || syncRequested)) {
    if (!ntpKicked || ms - lastNtpKick >= RETRY_MS) {
      // Registers SNTP without waiting for a response. Periodic refresh stays enabled.
      esp_sntp_set_sync_interval(watchsync::NTP_INTERVAL_MS);
      // Configure SNTP without rewriting TZ while the radio task is encoding a frame.
      if (esp_sntp_enabled()) esp_sntp_stop();
      esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
      esp_sntp_setservername(0, NTP_PRIMARY);
      esp_sntp_setservername(1, NTP_FALLBACK);
      esp_sntp_setservername(2, nullptr);
      esp_sntp_init();
      ntpConfigured = ntpKicked = true;
      syncRequested = false;
      lastNtpKick = ms;
    }
  }
  wasConnected = connected;
}

const char *stateName(const Control &c, const watchsync::TimeTrust &t, const RadioStatus &r, int64_t us, time_t now) {
  if (c.updatingFirmware) return "updating";
  const bool session = sessionRequestedNow(c,r,us);
  if (!c.requested && !session) return "paused";
  if (r.error || timeZoneApplyFailed) return "error";
  if (c.changingTimeZone) return "configuring";
  if (!t.synced) return "waiting";
  if (!t.fresh(us)) return "stale";
  if (!session && !c.schedule.allows(now)) return "scheduled";
  if (r.active) return "transmitting";
  return "aligning";
}

String generateStatusJSON() {
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  const int64_t us = esp_timer_get_time();
  struct timeval tv; gettimeofday(&tv, nullptr);
  struct tm display = {};
  localtime_r(&tv.tv_sec, &display);
  char local[40], last[40];
  strftime(local, sizeof(local), "%Y-%m-%d %H:%M:%S", &display);
  last[0] = '\0';
  if (t.synced && timeZoneApplied) {
    struct tm lt; localtime_r(&t.lastSyncEpoch, &lt);
    strftime(last, sizeof(last), "%Y-%m-%d %H:%M:%S", &lt);
  }
  const bool session = sessionRequestedNow(c,r,us);
  const bool zoneReady = timeZoneApplied && !c.changingTimeZone;
  const bool scheduleOpen = zoneReady && t.synced && c.schedule.allows(tv.tv_sec);
  const time_t nextStart = zoneReady && t.synced ? c.schedule.nextStart(tv.tv_sec) : 0;
  const time_t windowEnd = zoneReady && t.synced ? c.schedule.currentWindow(tv.tv_sec).end : 0;
  char next[40] = {}, until[40] = {};
  struct tm scheduleTime = {};
  if (nextStart) { localtime_r(&nextStart,&scheduleTime); strftime(next,sizeof(next),"%Y-%m-%d %H:%M",&scheduleTime); }
  if (windowEnd) { localtime_r(&windowEnd,&scheduleTime); strftime(until,sizeof(until),"%Y-%m-%d %H:%M",&scheduleTime); }
  String s; s.reserve(1750);
  s += "{\"hostname\":\""; s += deviceConfiguration.hostname();
  s += "\",\"nextHostname\":\""; s += deviceConfiguration.nextHostname();
  s += "\",\"hostnamePending\":"; s += deviceConfiguration.pending() ? "true" : "false";
  s += ",\"version\":\"" VERSION "\",\"state\":\"";
  s += stateName(c,t,r,us,tv.tv_sec);
  s += "\",\"requested\":"; s += c.requested ? "true" : "false";
  s += ",\"active\":"; s += r.active && ((c.requested && scheduleOpen) || session) && t.fresh(us) && !c.changingTimeZone && !c.editingTime && !c.savingSettings && !c.updatingFirmware ? "true" : "false";
  s += ",\"otaEnabled\":"; s += "true";
  s += ",\"token\":\""; s += wifiConfiguration.controlToken(); s += '"';
  s += ",\"scheduleEnabled\":"; s += c.schedule.enabled ? "true" : "false";
  s += ",\"scheduleStart\":"; s += c.schedule.startMinute;
  s += ",\"scheduleEnd\":"; s += c.schedule.endMinute;
  s += ",\"nextBroadcast\":\""; s += next; s += '"';
  s += ",\"broadcastUntil\":\""; s += until; s += '"';
  s += ",\"sessionRequested\":"; s += session ? "true" : "false";
  s += ",\"sessionPending\":"; s += session && (r.sessionRevision != c.sessionRevision || r.sessionPending) ? "true" : "false";
  s += ",\"sessionRemainingSeconds\":";
  s += session && r.sessionRevision == c.sessionRevision && r.sessionEndsUs > us ? (long)((r.sessionEndsUs-us+999999)/1000000) : 0L;
  s += ",\"wifi\":"; s += WiFi.status() == WL_CONNECTED ? "true" : "false";
  s += ",\"ip\":\""; if (WiFi.status() == WL_CONNECTED) s += WiFi.localIP().toString(); s += '"';
  s += ",\"ntp\":"; s += ntpsync ? "true" : "false";
  s += ",\"ntpServers\":\""; s += NTP_PRIMARY; s += ", "; s += NTP_FALLBACK; s += '"';
  s += ",\"syncPending\":"; s += syncRequested ? "true" : "false";
  s += ",\"station\":"; s += c.station;
  s += ",\"stationName\":\""; s += stationNames[c.station]; s += '"';
  s += ",\"frequency\":"; s += r.frequency;
  s += ",\"targetFrequency\":"; s += st_cycle2[c.station] * 500;
  s += ",\"pin\":"; s += c.pin;
  s += ",\"drive\":"; s += (int)ANTENNA_DRIVE_STRENGTH;
  s += ",\"dst\":"; s += c.dst;
  s += ",\"timeZone\":\""; s += watchsync::TIME_ZONES[c.timeZone].id; s += '"';
  s += ",\"activeTimeZone\":\""; s += timeZoneApplied ? watchsync::TIME_ZONES[activeTimeZone].id : ""; s += '"';
  s += ",\"timeZoneName\":\""; s += timeZoneApplied ? watchsync::TIME_ZONES[activeTimeZone].name : "Unavailable"; s += '"';
  s += ",\"timeZoneRule\":\""; s += timeZoneApplied ? watchsync::TIME_ZONES[activeTimeZone].rule : ""; s += '"';
  s += ",\"timeZonePending\":"; s += c.changingTimeZone ? "true" : "false";
  s += ",\"timeZoneError\":"; s += timeZoneApplyFailed ? "true" : "false";
  s += ",\"led\":"; s += c.led ? "true" : "false";
  const char *lightNames[] = {"off", "green", "yellow", "red", "stopped", "blue", "purple"};
  s += ",\"ledState\":\""; s += lightNames[(int)r.light]; s += '"';
  s += ",\"setupAP\":"; s += wifiConfiguration.portalActive() ? "true" : "false";
  s += ",\"localTime\":\""; s += t.synced && timeZoneApplied ? local : ""; s += '"';
  s += ",\"localMilliseconds\":"; s += (long)(tv.tv_usec / 1000);
  s += ",\"lastSync\":\""; s += last; s += '"';
  s += ",\"syncAgeSeconds\":"; s += t.synced ? (long)((us - t.lastSyncUs) / 1000000) : -1L;
  s += ",\"remainingSeconds\":"; s += t.remainingSeconds(us);
  s += ",\"heap\":"; s += ESP.getFreeHeap();
  s += ",\"minimumHeap\":"; s += ESP.getMinFreeHeap();
  s += ",\"largestHeapBlock\":"; s += ESP.getMaxAllocHeap();
  s += ",\"radioError\":"; s += r.error;
  s += '}';
  return s;
}

String generateStatusText() {
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  const int64_t us = esp_timer_get_time();
  String s; s.reserve(500);
  s += "watchsync " VERSION "\nState: "; s += stateName(c,t,r,us,time(nullptr));
  s += "\nHostname: "; s += deviceConfiguration.hostname();
  s += "\nHostname after restart: "; s += deviceConfiguration.nextHostname();
  s += "\nSchedule: "; s += c.schedule.enabled ? "daily" : "always on";
  s += "\nSchedule start/end minutes: "; s += c.schedule.startMinute; s += '/'; s += c.schedule.endMinute;
  s += "\nTemporary pass requested: "; s += sessionRequestedNow(c,r,us) ? "yes" : "no";
  s += "\nStation: "; s += stationNames[c.station];
  s += "\nCarrier configured: "; s += r.frequency; s += " Hz";
  s += "\nCarrier requested: "; s += st_cycle2[c.station] * 500; s += " Hz";
  s += "\nGPIO: "; s += c.pin;
  s += "\nNTP sync confirmed: "; s += t.synced ? "yes" : "no";
  s += "\nHoldover remaining: "; s += t.remainingSeconds(us); s += " seconds";
  s += "\nTime zone: "; s += timeZoneApplied ? watchsync::TIME_ZONES[activeTimeZone].name : "Unavailable";
  s += "\nTime zone rule: "; s += timeZoneApplied ? watchsync::TIME_ZONES[activeTimeZone].rule : "";
  s += "\nSelected time zone: "; s += watchsync::TIME_ZONES[c.timeZone].id;
  s += "\nTime zone pending: "; s += c.changingTimeZone ? "yes" : "no";
  s += "\nTime zone error: "; s += timeZoneApplyFailed ? "yes" : "no";
  s += "\nFree heap: "; s += ESP.getFreeHeap(); s += " bytes";
  s += "\nMinimum free heap: "; s += ESP.getMinFreeHeap(); s += " bytes";
  s += "\nLargest free heap block: "; s += ESP.getMaxAllocHeap(); s += " bytes";
  s += '\n';
  return s;
}

void IRAM_ATTR meas_isr() { measCount = measCount + 1; }

int docmd(char *buf) {
  const size_t n = strlen(buf);
  if (!n) return 0;
  lastCmdResp = "";
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  const char command = tolower((unsigned char)buf[0]);
  bool save = false;
  if (command == 'h' && n == 1) {
    Serial.print(generateStatusText());
    Serial.println("e0/e1 pause/resume; u sync now; sX station (j,k,w,d,t,m,c); pNN pin; l0/l1 LED; y0/y1 NTP; x0..2 DST; f frequency test; r defaults.");
    return 1;
  } else if (command == 'u' && n == 1) {
    save = !ntpsync; requestSync(); lastCmdResp = "Time sync requested";
  } else if (command == 'e' && n == 2 && (buf[1] == '0' || buf[1] == '1')) {
    c.requested = buf[1] == '1'; save = true;
    if (!c.requested) { c.sessionRequested = false; ++c.sessionRevision; }
  } else if (command == 'l' && n == 2 && (buf[1] == '0' || buf[1] == '1')) {
    c.led = buf[1] == '1'; save = true;
  } else if (command == 'x' && n == 2 && buf[1] >= '0' && buf[1] <= '2') {
    c.dst = buf[1] - '0'; save = true;
  } else if (command == 's' && n == 2) {
    const char *p = strchr(stationCmds, tolower((unsigned char)buf[1]));
    if (!p) return 0;
    c.station = p - stationCmds; save = true;
  } else if (command == 'p' && n >= 2 && n <= 3) {
    for (size_t i=1;i<n;++i) if (!isdigit((unsigned char)buf[i])) return 0;
    const int pin = atoi(buf+1); if (!safePin(pin)) return 0;
    c.pin = pin; save = true;
  } else if (command == 'y' && n == 2 && (buf[1] == '0' || buf[1] == '1')) {
    ntpsync = buf[1] == '1';
    if (ntpsync) requestSync();
    else { esp_sntp_stop(); ntpConfigured = false; syncRequested = false; }
    save = true;
  } else if (command == 'f' && n == 1) {
    if (!r.active || !t.fresh(esp_timer_get_time()) || c.pin == PIN_MEAS) return 0;
    pinMode(PIN_MEAS, INPUT_PULLDOWN); measCount = 0;
    portENTER_CRITICAL(&stateMux); control.testCarrier = true; portEXIT_CRITICAL(&stateMux);
    attachInterrupt(digitalPinToInterrupt(PIN_MEAS), meas_isr, RISING);
    delay(200);  // radioTask remains independent of this diagnostic wait
    detachInterrupt(digitalPinToInterrupt(PIN_MEAS));
    portENTER_CRITICAL(&stateMux); control.testCarrier = false; portEXIT_CRITICAL(&stateMux);
    lastCmdResp = String("Measured approximately ") + String(measCount * 5) + " Hz; interrupts modulation during test";
    return 1;
  } else if (command == 'r' && n == 1) {
    c = Control(); requestSync(); save = true;
  } else if ((command == 'd' || command == 't') && n == 7) {
    if (c.changingTimeZone) return 0;
    for (size_t i=1;i<n;++i) if (!isdigit((unsigned char)buf[i])) return 0;
    time_t epoch = time(nullptr); struct tm edited; localtime_r(&epoch,&edited);
    const int a = (buf[1]-'0')*10+buf[2]-'0', b=(buf[3]-'0')*10+buf[4]-'0', d=(buf[5]-'0')*10+buf[6]-'0';
    if (command == 'd') {
      if (b<1||b>12||d<1||d>31) return 0;
      edited.tm_year=a+100; edited.tm_mon=b-1; edited.tm_mday=d;
    } else {
      if(a>23||b>59||d>59) return 0;
      edited.tm_hour=a; edited.tm_min=b; edited.tm_sec=d;
    }
    edited.tm_isdst=-1;
    // An arbitrary clock edit must not inherit trust from an earlier NTP sync.
    portENTER_CRITICAL(&stateMux);
    control.editingTime = true;
    ++timeEditGeneration;
    timeTrust.synced = false;
    portEXIT_CRITICAL(&stateMux);
    struct timeval tv = {}; tv.tv_sec=mktime(&edited); settimeofday(&tv,nullptr);
    portENTER_CRITICAL(&stateMux);
    timeTrust.synced = false;
    ++timeEditGeneration;
    control.editingTime = false;
    portEXIT_CRITICAL(&stateMux);
    if(ntpsync) requestSync();
    lastCmdResp="Manual time set; a new NTP sync is required before transmission";
    return 1;
  } else if ((command == 'z' || command == 'n') && n == 2 && (buf[1]=='0'||buf[1]=='1')) {
    lastCmdResp="Legacy option has no effect on Atom Lite";
    return 1;
  } else return 0;
  if (save) {
    updateControl(c);
    if (!saveSettings()) { lastCmdResp="Change applied for this run, but settings could not be saved."; return 0; }
  }
  return 1;
}

bool parseField(const char *name, int &value) {
  if (!server.hasArg(name)) return false;
  const String s = server.arg(name);
  if (s.length() == 0 || s.length() > 3) return false;
  for (size_t i=0; i<s.length(); ++i) if(!isdigit((unsigned char)s[i])) return false;
  value=s.toInt(); return true;
}

void handleSettings() {
  int station,led,dst;
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  int pin = c.pin;  // The web interface displays the configured GPIO without changing it.
  int zone = c.timeZone;  // Older API clients may omit the new field.
  if (server.hasArg("timezone")) {
    const String id = server.arg("timezone");
    zone = id.length() <= 32 ? watchsync::findTimeZone(id.c_str()) : -1;
  }
  if(!parseField("station",station)||(server.hasArg("pin")&&!parseField("pin",pin))||
     !parseField("led",led)||!parseField("dst",dst)||station>SN_BPC||!safePin(pin)||led>1||dst>2||zone<0) {
    server.send(400,"text/plain","Invalid settings"); return;
  }
  c.station=station; c.pin=pin;
  c.led=led!=0; c.dst=dst; c.timeZone=zone;
  updateControl(c);
  if (!saveSettings()) { server.send(500,"application/json","{\"message\":\"Settings applied for this run, but could not be saved. Retry before restarting.\"}"); return; }
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json; charset=utf-8",generateStatusJSON());
}

void handleSchedule() {
  if (!wifiConfiguration.controlWriteAllowed()) return;
  int enabled, start, end;
  if (!parseField("enabled",enabled) || enabled > 1 || !server.hasArg("start") || !server.hasArg("end")) {
    server.send(400,"application/json","{\"message\":\"Invalid schedule.\"}"); return;
  }
  // Schedule minutes need four digits; the radio field parser deliberately allows three.
  const String startText=server.arg("start"), endText=server.arg("end");
  if (startText.isEmpty() || endText.isEmpty() || startText.length()>4 || endText.length()>4) {
    server.send(400,"application/json","{\"message\":\"Invalid start or end time.\"}"); return;
  }
  for (unsigned i=0;i<startText.length();++i) if (!isdigit((unsigned char)startText[i])) {
    server.send(400,"application/json","{\"message\":\"Invalid start time.\"}"); return;
  }
  for (unsigned i=0;i<endText.length();++i) if (!isdigit((unsigned char)endText[i])) {
    server.send(400,"application/json","{\"message\":\"Invalid end time.\"}"); return;
  }
  start=startText.toInt(); end=endText.toInt();
  if (start>=1440 || end>=1440 || start==end) {
    server.send(400,"application/json","{\"message\":\"Choose different start and end times. Use Always on for continuous transmission.\"}"); return;
  }
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  c.schedule.enabled=enabled!=0; c.schedule.startMinute=start; c.schedule.endMinute=end;
  updateControl(c);
  if (!saveSettings()) { server.send(500,"application/json","{\"message\":\"Schedule applied for this run, but could not be saved. Retry before restarting.\"}"); return; }
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json; charset=utf-8",generateStatusJSON());
}

void handleSession() {
  if (!wifiConfiguration.controlWriteAllowed()) return;
  const String action=server.arg("action");
  if (action!="start" && action!="stop") { server.send(400,"application/json","{\"message\":\"Invalid session action.\"}"); return; }
  setTimedSession(action=="start");
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json; charset=utf-8",generateStatusJSON());
}

bool deviceWriteAllowed() {
  if (!wifiConfiguration.controlWriteAllowed()) return false;
  if (!wifiConfiguration.localHostAllowed(server.hostHeader()) || !server.hasHeader("Origin") ||
      server.header("Origin") != String("http://") + server.hostHeader()) {
    server.send(403,"application/json","{\"message\":\"Open this device page and try again.\"}"); return false;
  }
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  if (deviceRestartPending || c.updatingFirmware || c.savingSettings || c.changingTimeZone || c.editingTime
      || !firmwareUpdate.deviceChangesAllowed()

      ) {
    server.send(409,"application/json","{\"message\":\"Wait for startup checks and current operations to finish.\"}"); return false;
  }
  return true;
}

void handleDeviceSettings() {
  if (!deviceWriteAllowed()) return;
  String hostname=server.arg("hostname"); hostname.toLowerCase();
  const String restart=server.arg("restart");
  if (!watchsync::validHostname(hostname.c_str(),hostname.length()) || (restart!="0" && restart!="1")) {
    server.send(400,"application/json","{\"message\":\"Use 1-32 lowercase letters, digits or hyphens; start and end with a letter or digit.\"}"); return;
  }
  if (!pauseForStorage()) { server.send(503,"application/json","{\"message\":\"Could not pause transmission. Try again.\"}"); return; }
  const bool saved=deviceConfiguration.saveHostname(hostname.c_str(),hostname.length());
  if (saved && restart=="1") { deviceRestartPending=true; deviceRestartAt=millis(); }
  else resumeAfterStorage();
  if (!saved) { server.send(500,"application/json","{\"message\":\"Could not save the hostname. Try again.\"}"); return; }
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json; charset=utf-8",generateStatusJSON());
}

void handleFactoryReset() {
  if (!deviceWriteAllowed()) return;
  if (server.arg("confirm")!="RESET") { server.send(400,"application/json","{\"message\":\"Type RESET to confirm factory reset.\"}"); return; }
  if (!pauseForStorage()) { server.send(503,"application/json","{\"message\":\"Could not pause transmission. Try again.\"}"); return; }
  if (!deviceConfiguration.requestFactoryReset()) {
    resumeAfterStorage(); server.send(500,"application/json","{\"message\":\"Could not save the reset request. Nothing was reset.\"}"); return;
  }
  // Hold the acknowledged radio pause until reboot. No more controls or storage writes.
  deviceRestartPending=true; deviceRestartAt=millis();
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json","{\"message\":\"Factory reset saved. Restarting into Wi-Fi setup.\"}");
}

String generateDiagnostics() {
  Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
  const int64_t us = esp_timer_get_time();
  char uptime[32]; snprintf(uptime,sizeof(uptime),"%llu",(unsigned long long)(us/1000000));
  const esp_reset_reason_t reason = esp_reset_reason();
  const char *reset = reason == ESP_RST_POWERON ? "Power on" : reason == ESP_RST_SW ? "Software restart" :
      reason == ESP_RST_BROWNOUT ? "Brownout" : reason == ESP_RST_PANIC ? "Panic" :
      reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT ? "Watchdog" : "Other";
  String s; s.reserve(700);
  s = "watchsync " VERSION "\nTarget: Atom Lite, GPIO25\nUptime seconds: "; s += uptime;
  s += "\nReset reason: "; s += reset; s += " ("; s += (int)reason; s += ')';
  s += "\nState: "; s += stateName(c,t,r,us,time(nullptr));
  s += "\nWi-Fi: "; s += WiFi.status()==WL_CONNECTED ? "connected" : "disconnected";
  if (WiFi.status()==WL_CONNECTED) { s += "\nWi-Fi RSSI dBm: "; s += WiFi.RSSI(); }
  s += "\nLast Wi-Fi disconnect reason code: "; s += wifiConfiguration.lastDisconnectReason();
  s += "\nConfigured NTP servers: "; s += NTP_PRIMARY; s += ", "; s += NTP_FALLBACK;
  s += "\nConfirmed time age seconds: "; s += t.synced ? (long)((us-t.lastSyncUs)/1000000) : -1L;
  s += "\nFree heap bytes: "; s += ESP.getFreeHeap();
  s += "\nMinimum heap bytes: "; s += ESP.getMinFreeHeap();
  s += "\nLargest heap block bytes: "; s += ESP.getMaxAllocHeap();
  s += "\nRadio error code: "; s += r.error; s += '\n';
  return s;
}

void setupWebServer() {
  const char *headers[]={"Origin"};
  server.collectHeaders(headers,1);
  server.on("/api/device",HTTP_POST,handleDeviceSettings);
  server.on("/api/device/reset",HTTP_POST,handleFactoryReset);
  wifiConfiguration.registerRoutes();
  server.on("/api/schedule", HTTP_POST, handleSchedule);
  server.on("/api/session", HTTP_POST, handleSession);
  server.on("/api/timezones", HTTP_GET, [](){
    String s; s.reserve(1500);
    s = "[";
    for (size_t i=0; i<watchsync::TIME_ZONE_COUNT; ++i) {
      if (i) s += ',';
      s += "{\"id\":\""; s += watchsync::TIME_ZONES[i].id;
      s += "\",\"name\":\""; s += watchsync::TIME_ZONES[i].name; s += "\"}";
    }
    s += ']';
    server.sendHeader("Cache-Control", "no-store");
    server.send(200,"application/json; charset=utf-8",s);
  });
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/", HTTP_GET, [](){
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", wifiConfiguration.portalClient() ? WIFI_SETUP_UI : WEB_UI);
  });
  server.on("/api/status", HTTP_GET, [](){
    server.sendHeader("Cache-Control","no-store");
    server.send(200,"application/json; charset=utf-8",generateStatusJSON());
  });
  server.on("/api/diagnostics", HTTP_GET, [](){
    if (!wifiConfiguration.lanClient()) { server.send(403,"text/plain","Open Help through your normal Wi-Fi network."); return; }
    server.sendHeader("Cache-Control","no-store");
    server.send(200,"text/plain; charset=utf-8",generateDiagnostics());
  });
  server.on("/status.txt", HTTP_GET, [](){ server.send(200,"text/plain; charset=utf-8",generateStatusText()); });
  server.on("/cmd", HTTP_ANY, [](){
    if (!server.hasArg("c") || server.arg("c").length()==0 || server.arg("c").length()>126) {
      server.send(400,"text/plain","ERR"); return;
    }
    char buf[128]; server.arg("c").toCharArray(buf,sizeof(buf));
    const bool ok=docmd(buf);
    server.send(ok?200:400,"text/plain; charset=utf-8",ok?String("OK\n")+lastCmdResp:String("ERR\n"));
  });
  server.onNotFound([](){
    // Windows opens /redirect; captive probes and arbitrary landing pages must reach setup.
    if (wifiConfiguration.portalClient() && server.method()==HTTP_GET && !server.uri().startsWith("/api/")) {
      server.sendHeader("Cache-Control","no-store");
      server.sendHeader("Location",String("http://")+WiFi.softAPIP().toString()+"/");
      server.send(302,"text/plain",""); return;
    }
    server.send(404,"text/plain","Not found");
  });
  server.begin();
}

void setup() {
  Serial.begin(115200);
#if PIN_LED >= 0
  pinMode(PIN_LED,OUTPUT);
#endif
#if PIN_BUTTON >= 0
  // GPIO34-39 have no internal pull-ups; Atom Lite supplies its own on GPIO39.
  pinMode(PIN_BUTTON, PIN_BUTTON >= 34 ? INPUT : INPUT_PULLUP);
#endif
  indicator(watchsync::StatusLight::Off);
  wifiConfiguration.setStorageHooks(pauseForStorage,resumeAfterStorage);
  // Finish a requested reset before loading either current or legacy settings.
  if (!deviceConfiguration.begin()) {
    Serial.println("Device settings could not be initialized. Transmission remains stopped; retrying startup.");
    delay(3000); ESP.restart(); return;
  }
  loadSettings();
  timeZoneApplyFailed = !applyTimeZone(control.timeZone);
  control.changingTimeZone = timeZoneApplyFailed;
  control.timeZoneError = timeZoneApplyFailed;
  pinMode(control.pin,INPUT);
  esp_sntp_set_time_sync_notification_cb(onTimeSync);
  wifiConfiguration.begin();

  setupWebServer();
  firmwareUpdate.begin(pauseForFirmware, resumeAfterFirmware);

  const BaseType_t taskOk=xTaskCreatePinnedToCore(radioTask,"radio",6144,nullptr,4,&radioTaskHandle,1);
  if(taskOk!=pdPASS) { RadioStatus r; r.error=ESP_ERR_NO_MEM; publishRadio(r); }
  Serial.println("watchsync " VERSION " ready. Waiting for confirmed NTP time.");
  Serial.println("Firmware profile: portal-only");

  Serial.print("http://"); Serial.print(deviceConfiguration.hostname());
  Serial.println(".local  |  press: pause/resume; double press: one-hour pass; hold 3s: time sync");
}

void loop() {
  if (deviceRestartPending) {
    if ((uint32_t)(millis()-deviceRestartAt)>=1500) ESP.restart();
    delay(1); return;
  }
  firmwareUpdate.service(firmwareStartupHealthy());
  if (firmwareUpdate.restarting()) { delay(1); return; }

  serviceTimeZone();
  serviceNetwork();
  serviceSession();
  bool pressed = false;
#if PIN_BUTTON >= 0
  pressed = digitalRead(PIN_BUTTON) == LOW;
#endif
  const uint32_t buttonMs = millis();
  const auto action=button.update(buttonInput.update(pressed,buttonMs),buttonMs);
  if(action==watchsync::ButtonAction::Toggle) {
    toggleTransmission();
  } else if(action==watchsync::ButtonAction::Sync) {
    const bool changed=!ntpsync; requestSync(); if(changed) saveSettings();
  } else if(action==watchsync::ButtonAction::TimedSession) {
    Control c; watchsync::TimeTrust t; RadioStatus r; snapshot(c,t,r);
    setTimedSession(!sessionRequestedNow(c,r,esp_timer_get_time()));
  }
  static char command[128]; static size_t length=0; static bool overflow=false;
  while(Serial.available()) {
    const char c=Serial.read();
    if(c=='\n'||c=='\r') {
      if(!overflow && length) { command[length]='\0'; if(!docmd(command)) Serial.println("ERR"); }
      length=0; overflow=false;
    } else if(length<sizeof(command)-1) command[length++]=c;
    else overflow=true;
  }
  server.handleClient();
  delay(1);
}
