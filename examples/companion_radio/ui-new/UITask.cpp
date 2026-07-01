#include "UITask.h"
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"
#include <pinyin_simple_backend.h>
#ifdef WIFI_SSID
  #include <WiFi.h>
#endif

extern DataStore store;

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     15000   // 15 seconds
#endif
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200

#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif

#if UI_HAS_JOYSTICK
  #define PRESS_LABEL "press Enter"
#else
  #define PRESS_LABEL "long press"
#endif

static const char* PRESET_MSG_STORE_FILE = "/ui_presets";
static const char* REGION_IDX_STORE_FILE = "/ui_region";
static const uint8_t PRESET_MSG_STORE_KEY[] = { 'G', 'A', 'T', 'P', 'M', 'S', 'G', '1' };
static const uint8_t PRESET_MSG_STORE_VERSION = 1;
static const uint8_t OLD_PRESET_MSG_COUNT = 4;
static const uint8_t OLD_PRESET_MSG_BLOB_BYTES = 1 + OLD_PRESET_MSG_COUNT * UI_PRESET_MSG_BYTES;

static File openUiWrite(const char* filename) {
  FILESYSTEM* fs = store.getPrimaryFS();
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  fs->remove(filename);
  return fs->open(filename, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "w");
#else
  return fs->open(filename, "w", true);
#endif
}

struct RegionPreset {
  const char* name;
  float freq;
  float bw;
  uint8_t sf;
  uint8_t cr;
};

static const RegionPreset REGION_PRESETS[] = {
  { "US",      902.125f, 250.0f, 11, 5 },
  { "EU 433", 433.125f, 250.0f, 11, 5 },
  { "EU 868", 869.525f, 250.0f, 11, 5 },
  { "CN",      480.375f, 250.0f, 11, 5 },
  { "JP",      920.625f, 250.0f, 11, 5 },
  { "ANZ",     915.125f, 250.0f, 11, 5 },
  { "ANZ433",  433.175f, 250.0f, 11, 5 },
  { "RU",      868.825f, 250.0f, 11, 5 },
  { "KR",      920.125f, 250.0f, 11, 5 },
  { "TW",      920.125f, 250.0f, 11, 5 },
  { "IN",      865.125f, 250.0f, 11, 5 },
  { "NZ 865",  864.125f, 250.0f, 11, 5 },
  { "TH",      920.125f, 250.0f, 11, 5 },
  { "UA 433",  433.125f, 250.0f, 11, 5 },
  { "UA 868",  868.125f, 250.0f, 11, 5 },
  { "MY 433",  433.125f, 250.0f, 11, 5 },
  { "MY 919",  919.125f, 250.0f, 11, 5 },
  { "SG 923",  917.125f, 250.0f, 11, 5 },
  { "PH 433",  433.125f, 250.0f, 11, 5 },
  { "PH 868",  868.125f, 250.0f, 11, 5 },
  { "PH 915",  915.125f, 250.0f, 11, 5 },
  { "KZ 433",  433.200f, 250.0f, 11, 5 },
  { "KZ 863",  863.125f, 250.0f, 11, 5 },
  { "NP 865",  865.125f, 250.0f, 11, 5 },
  { "BR 902",  902.125f, 250.0f, 11, 5 },
};

static const uint8_t REGION_PRESET_COUNT = sizeof(REGION_PRESETS) / sizeof(REGION_PRESETS[0]);

static uint8_t closestRegionPreset(float freq) {
  uint8_t best = 0;
  float best_delta = 9999.0f;
  for (uint8_t i = 0; i < REGION_PRESET_COUNT; i++) {
    float delta = freq > REGION_PRESETS[i].freq ? freq - REGION_PRESETS[i].freq : REGION_PRESETS[i].freq - freq;
    if (delta < best_delta) {
      best_delta = delta;
      best = i;
    }
  }
  return best;
}

static uint8_t loadRegionPresetIndex(float freq) {
  uint8_t idx = 0xFF;
  File f = store.openRead(REGION_IDX_STORE_FILE);
  if (f) {
    if (f.read(&idx, 1) != 1) idx = 0xFF;
    f.close();
  }
  if (idx < REGION_PRESET_COUNT) {
    const RegionPreset& r = REGION_PRESETS[idx];
    float delta = freq > r.freq ? freq - r.freq : r.freq - freq;
    if (delta < 0.001f) return idx;
  }
  return closestRegionPreset(freq);
}

static void saveRegionPresetIndex(uint8_t idx) {
  File f = openUiWrite(REGION_IDX_STORE_FILE);
  if (f) {
    f.write(&idx, 1);
    f.close();
  }
}

struct TimezoneOption {
  const char* label;
  int16_t offset_minutes;
};

static const TimezoneOption TIMEZONE_OPTIONS[] = {
  { "UTC-12", -720 },
  { "UTC-11", -660 },
  { "UTC-10", -600 },
  { "UTC-9",  -540 },
  { "UTC-8",  -480 },
  { "UTC-7",  -420 },
  { "UTC-6",  -360 },
  { "UTC-5",  -300 },
  { "UTC-4",  -240 },
  { "UTC-3",  -180 },
  { "UTC-2",  -120 },
  { "UTC-1",   -60 },
  { "UTC",        0 },
  { "UTC+1",     60 },
  { "UTC+2",    120 },
  { "UTC+3",    180 },
  { "UTC+4",    240 },
  { "UTC+5",    300 },
  { "UTC+6",    360 },
  { "UTC+7",    420 },
  { "UTC+8",    480 },
  { "UTC+9",    540 },
  { "UTC+10",   600 },
  { "UTC+11",   660 },
  { "UTC+12",   720 },
  { "UTC+13",   780 },
  { "UTC+14",   840 },
};

static const uint8_t TIMEZONE_OPTION_COUNT = sizeof(TIMEZONE_OPTIONS) / sizeof(TIMEZONE_OPTIONS[0]);

static uint8_t defaultTimezoneIndex() {
  for (uint8_t i = 0; i < TIMEZONE_OPTION_COUNT; i++) {
    if (TIMEZONE_OPTIONS[i].offset_minutes == 480) return i;
  }
  return 0;
}

struct ClockParts {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

static bool isLeapYear(uint16_t year) {
  return ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
}

static uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t DAYS[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (month == 2 && isLeapYear(year)) return 29;
  return DAYS[month - 1];
}

static void epochToClockParts(uint32_t epoch, int16_t timezone_minutes, ClockParts& out) {
  int64_t adjusted = (int64_t)epoch + (int64_t)timezone_minutes * 60;
  if (adjusted < 0) adjusted = 0;

  uint32_t days = adjusted / 86400;
  uint32_t seconds = adjusted % 86400;

  out.hour = seconds / 3600;
  seconds %= 3600;
  out.minute = seconds / 60;
  out.second = seconds % 60;

  uint16_t year = 1970;
  while (true) {
    uint16_t year_days = isLeapYear(year) ? 366 : 365;
    if (days < year_days) break;
    days -= year_days;
    year++;
  }

  uint8_t month = 1;
  while (true) {
    uint8_t month_days = daysInMonth(year, month);
    if (days < month_days) break;
    days -= month_days;
    month++;
  }

  out.year = year;
  out.month = month;
  out.day = days + 1;
}

#include "icons.h"

class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[20];

public:
  SplashScreen(UITask* task) : _task(task) {
    // strip off dash and commit hash by changing dash to null terminator
    // e.g: v1.2.3-abcdef -> v1.2.3
    const char *ver = FIRMWARE_VERSION;
    if (ver[0] == 'v') ver++;
    const char *dash = strchr(ver, '-');

    int len = dash ? dash - ver : strlen(ver);
    if (len >= 4 && strcmp(ver + len - 2, ".0") == 0) len -= 2;
    strcpy(_version_info, "MeshOS ");
    int prefix_len = strlen(_version_info);
    if (len >= (int)sizeof(_version_info) - prefix_len) len = sizeof(_version_info) - prefix_len - 1;
    memcpy(_version_info + prefix_len, ver, len);
    _version_info[prefix_len + len] = 0;

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(2);
    display.drawTextCentered(display.width()/2, 8, "MESHCORE");

    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 32, _version_info);

    display.drawTextCentered(display.width()/2, 49, "meshcore.co.uk");

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};

class HomeScreen : public UIScreen {
  enum ConnectionMode {
    MODE_BLE,
    MODE_USB,
  };

  enum HomePage {
    FIRST,
    COMPOSE,
    RECENT,
    RADIO,
    REGION,
    BLUETOOTH,
    ADVERT,
#if ENV_INCLUDE_GPS == 1
    GPS,
#endif
#if UI_SENSORS_PAGE == 1
    SENSORS,
#endif
    CLOCK,
    SHUTDOWN,
    Count    // keep as last
  };

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
  uint8_t _region_idx;
  uint8_t _conn_mode;
  uint8_t _conn_cursor;
  uint8_t _tz_idx;
  uint8_t _preset_idx;
  bool _compose_presets;
  bool _shutdown_init;
  AdvertPath recent[UI_RECENT_LIST_SIZE];


  void renderBatteryIndicator(DisplayDriver& display, uint16_t batteryMilliVolts) {
    // Convert millivolts to percentage
#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif
    const int minMilliVolts = BATT_MIN_MILLIVOLTS;
    const int maxMilliVolts = BATT_MAX_MILLIVOLTS;
    int batteryPercentage = ((batteryMilliVolts - minMilliVolts) * 100) / (maxMilliVolts - minMilliVolts);
    if (batteryPercentage < 0) batteryPercentage = 0; // Clamp to 0%
    if (batteryPercentage > 100) batteryPercentage = 100; // Clamp to 100%

    // battery icon
    int iconWidth = 24;
    int iconHeight = 10;
    int iconX = display.width() - iconWidth - 5; // Position the icon near the top-right corner
    int iconY = 0;
    display.setColor(DisplayDriver::GREEN);

    // battery outline
    display.drawRect(iconX, iconY, iconWidth, iconHeight);

    // battery "cap"
    display.fillRect(iconX + iconWidth, iconY + (iconHeight / 4), 3, iconHeight / 2);

    // fill the battery based on the percentage
    int fillWidth = (batteryPercentage * (iconWidth - 4)) / 100;
    display.fillRect(iconX + 2, iconY + 2, fillWidth, iconHeight - 4);

    // show muted icon if buzzer is muted
#ifdef PIN_BUZZER
    if (_task->isBuzzerQuiet()) {
      display.setColor(DisplayDriver::RED);
      display.drawXbm(iconX - 9, iconY + 1, muted_icon, 8, 8);
    }
#endif
  }

  CayenneLPP sensors_lpp;
  int sensors_nb = 0;
  bool sensors_scroll = false;
  int sensors_scroll_offset = 0;
  int next_sensors_refresh = 0;

  void refresh_sensors() {
    if (millis() > next_sensors_refresh) {
      sensors_lpp.reset();
      sensors_nb = 0;
      sensors_lpp.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      sensors.querySensors(0xFF, sensors_lpp);
      LPPReader reader (sensors_lpp.getBuffer(), sensors_lpp.getSize());
      uint8_t channel, type;
      while(reader.readHeader(channel, type)) {
        reader.skipData(type);
        sensors_nb ++;
      }
      sensors_scroll = sensors_nb > UI_RECENT_LIST_SIZE;
#if AUTO_OFF_MILLIS > 0
      next_sensors_refresh = millis() + 5000; // refresh sensor values every 5 sec
#else
      next_sensors_refresh = millis() + 60000; // refresh sensor values every 1 min
#endif
    }
  }

public:
  HomeScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors, NodePrefs* node_prefs)
     : _task(task), _rtc(rtc), _sensors(sensors), _node_prefs(node_prefs), _page(0),
       _region_idx(loadRegionPresetIndex(node_prefs->freq)),
       _conn_mode(MODE_BLE), _conn_cursor(MODE_BLE),
       _tz_idx(defaultTimezoneIndex()),
       _preset_idx(0), _compose_presets(false),
       _shutdown_init(false), sensors_lpp(200) {  }

  void poll() override {
    if (_shutdown_init && !_task->isButtonPressed()) {  // must wait for USR button to be released
      _task->shutdown();
    }
  }

  int render(DisplayDriver& display) override {
    char tmp[80];
    // node name
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    char filtered_name[sizeof(_node_prefs->node_name)];
    display.translateUTF8ToBlocks(filtered_name, _node_prefs->node_name, sizeof(filtered_name));
    display.setCursor(0, 0);
    display.print(filtered_name);

    // battery voltage
    renderBatteryIndicator(display, _task->getBattMilliVolts());

    // curr page indicator
    int y = 14;
    int x = display.width() / 2 - 5 * (HomePage::Count-1);
    for (uint8_t i = 0; i < HomePage::Count; i++, x += 10) {
      if (i == _page) {
        display.fillRect(x-1, y-1, 3, 3);
      } else {
        display.fillRect(x, y, 1, 1);
      }
    }

    if (_page == HomePage::FIRST) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(2);
      sprintf(tmp, "MSG: %d", _task->getMsgCount());
      display.drawTextCentered(display.width() / 2, 20, tmp);

      #ifdef WIFI_SSID
        IPAddress ip = WiFi.localIP();
        snprintf(tmp, sizeof(tmp), "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 54, tmp);
      #endif
      display.setColor(DisplayDriver::GREEN);
      display.setTextSize(1);
      if (_conn_mode == MODE_USB) {
        display.drawTextCentered(display.width() / 2, 46, "USB mode");
      } else if (_task->hasConnection()) {
        display.setColor(DisplayDriver::GREEN);
        display.drawTextCentered(display.width() / 2, 46, "BLE mode");
      } else if (the_mesh.getBLEPin() != 0) { // BT pin
        display.setColor(DisplayDriver::RED);
        display.setTextSize(2);
        sprintf(tmp, "Pin:%06lu", (unsigned long)the_mesh.getBLEPin());
        display.drawTextCentered(display.width() / 2, 43, tmp);
      }
    } else if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);
      display.setColor(DisplayDriver::GREEN);
      int y = 20;
      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, y += 11) {
        auto a = &recent[i];
        if (a->name[0] == 0) continue;  // empty slot
        int secs = _rtc->getCurrentTime() - a->recv_timestamp;
        if (secs < 60) {
          sprintf(tmp, "%ds", secs);
        } else if (secs < 60*60) {
          sprintf(tmp, "%dm", secs / 60);
        } else {
          sprintf(tmp, "%dh", secs / (60*60));
        }

        int timestamp_width = display.getTextWidth(tmp);
        int max_name_width = display.width() - timestamp_width - 1;

        char filtered_recent_name[sizeof(a->name)];
        display.translateUTF8ToBlocks(filtered_recent_name, a->name, sizeof(filtered_recent_name));
        display.drawTextEllipsized(0, y, max_name_width, filtered_recent_name);
        display.setCursor(display.width() - timestamp_width - 1, y);
        display.print(tmp);
      }
    } else if (_page == HomePage::RADIO) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(1);
      // freq / sf
      display.setCursor(0, 20);
      sprintf(tmp, "FQ: %06.3f   SF: %d", _node_prefs->freq, _node_prefs->sf);
      display.print(tmp);

      display.setCursor(0, 31);
      sprintf(tmp, "BW: %03.2f     CR: %d", _node_prefs->bw, _node_prefs->cr);
      display.print(tmp);

      // tx power,  noise floor
      display.setCursor(0, 42);
      sprintf(tmp, "TX: %ddBm", _node_prefs->tx_power_dbm);
      display.print(tmp);
      display.setCursor(0, 53);
      sprintf(tmp, "Noise floor: %d", radio_driver.getNoiseFloor());
      display.print(tmp);
    } else if (_page == HomePage::REGION) {
      const RegionPreset& r = REGION_PRESETS[_region_idx];
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 18, "Region Preset");

      display.setTextSize(2);
      display.drawTextCentered(display.width() / 2, 29, r.name);

      display.setColor(DisplayDriver::GREEN);
      display.setTextSize(1);
      snprintf(tmp, sizeof(tmp), "%06.3fMHz SF%d", r.freq, r.sf);
      display.drawTextCentered(display.width() / 2, 48, tmp);
      display.drawTextCentered(display.width() / 2, 64 - 8, "Up/Down  Enter");
    } else if (_page == HomePage::BLUETOOTH) {
      display.setTextSize(1);
      display.setColor(DisplayDriver::GREEN);
      int cy = 27;
      display.drawTextCentered(display.width() / 2, cy, "BLE");
      display.drawTextCentered(display.width() / 2, cy + 14, "USB");
      display.setCursor(22, cy + (_conn_cursor == MODE_USB ? 14 : 0));
      display.print(_conn_mode == _conn_cursor ? "o" : ">");
      display.drawTextCentered(display.width() / 2, 64 - 10, "Connection Mode");
    } else if (_page == HomePage::ADVERT) {
      display.setColor(DisplayDriver::GREEN);
      display.drawXbm((display.width() - 32) / 2, 18, advert_icon, 32, 32);
      display.drawTextCentered(display.width() / 2, 64 - 11, "advert: " PRESS_LABEL);
    } else if (_page == HomePage::COMPOSE) {
      display.setTextSize(1);
      if (_compose_presets) {
        display.setColor(DisplayDriver::YELLOW);
        snprintf(tmp, sizeof(tmp), "Preset %u/%u", (unsigned)(_preset_idx + 1), (unsigned)_task->getPresetMessageCount());
        display.drawTextCentered(display.width() / 2, 18, tmp);

        const char* msg = _task->getPresetMessage(_preset_idx);
        display.setColor(DisplayDriver::LIGHT);
        if (msg[0]) {
          display.setCursor(0, 31);
          display.printWordWrap(msg, display.width());
        } else {
          display.drawTextCentered(display.width() / 2, 34, "(empty)");
        }
        display.setColor(DisplayDriver::GREEN);
        display.drawTextCentered(display.width() / 2, 64 - 9, "Enter send  Hold edit");
      } else {
        display.setColor(DisplayDriver::YELLOW);
        display.setTextSize(2);
        display.drawTextCentered(display.width() / 2, 22, "Compose");
        display.setColor(DisplayDriver::GREEN);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 43, "To: Public");
        display.drawTextCentered(display.width() / 2, 64 - 11, "Down presets");
      }
#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::GPS) {
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
      int y = 18;
      bool gps_state = _task->getGPSState();
      if (!gps_state && _node_prefs->gps_enabled) gps_state = true;
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state != hw_gps_state) {
        strcpy(buf, gps_state ? "gps off(hw)" : "gps off(sw)");
      } else {
        strcpy(buf, gps_state ? "gps on" : "gps off");
      }
#else
      strcpy(buf, gps_state ? "gps on" : "gps off");
#endif
      display.drawTextLeftAlign(0, y, buf);
      if (nmea == NULL) {
        y = y + 12;
        display.drawTextLeftAlign(0, y, "Can't access GPS");
      } else {
        strcpy(buf, nmea->isValid()?"fix":"no fix");
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "sat");
        sprintf(buf, "%d", nmea->satellitesCount());
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "pos");
        sprintf(buf, "%.4f %.4f",
          nmea->getLatitude()/1000000., nmea->getLongitude()/1000000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "alt");
        sprintf(buf, "%.2f", nmea->getAltitude()/1000.);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
      }
#endif
#if UI_SENSORS_PAGE == 1
    } else if (_page == HomePage::SENSORS) {
      int y = 18;
      refresh_sensors();
      char buf[30];
      char name[30];
      LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());

      for (int i = 0; i < sensors_scroll_offset; i++) {
        uint8_t channel, type;
        r.readHeader(channel, type);
        r.skipData(type);
      }

      for (int i = 0; i < (sensors_scroll?UI_RECENT_LIST_SIZE:sensors_nb); i++) {
        uint8_t channel, type;
        if (!r.readHeader(channel, type)) { // reached end, reset
          r.reset();
          r.readHeader(channel, type);
        }

        display.setCursor(0, y);
        float v;
        switch (type) {
          case LPP_GPS: // GPS
            float lat, lon, alt;
            r.readGPS(lat, lon, alt);
            strcpy(name, "gps"); sprintf(buf, "%.4f %.4f", lat, lon);
            break;
          case LPP_VOLTAGE:
            r.readVoltage(v);
            strcpy(name, "voltage"); sprintf(buf, "%6.2f", v);
            break;
          case LPP_CURRENT:
            r.readCurrent(v);
            strcpy(name, "current"); sprintf(buf, "%.3f", v);
            break;
          case LPP_TEMPERATURE:
            r.readTemperature(v);
            strcpy(name, "temperature"); sprintf(buf, "%.2f", v);
            break;
          case LPP_RELATIVE_HUMIDITY:
            r.readRelativeHumidity(v);
            strcpy(name, "humidity"); sprintf(buf, "%.2f", v);
            break;
          case LPP_BAROMETRIC_PRESSURE:
            r.readPressure(v);
            strcpy(name, "pressure"); sprintf(buf, "%.2f", v);
            break;
          case LPP_ALTITUDE:
            r.readAltitude(v);
            strcpy(name, "altitude"); sprintf(buf, "%.0f", v);
            break;
          case LPP_POWER:
            r.readPower(v);
            strcpy(name, "power"); sprintf(buf, "%6.2f", v);
            break;
          default:
            r.skipData(type);
            strcpy(name, "unk"); sprintf(buf, "");
        }
        display.setCursor(0, y);
        display.print(name);
        display.setCursor(
          display.width()-display.getTextWidth(buf)-1, y
        );
        display.print(buf);
        y = y + 12;
      }
      if (sensors_scroll) sensors_scroll_offset = (sensors_scroll_offset+1)%sensors_nb;
      else sensors_scroll_offset = 0;
#endif
    } else if (_page == HomePage::CLOCK) {
      const TimezoneOption& tz = TIMEZONE_OPTIONS[_tz_idx];
      ClockParts clock;
      uint32_t now = _rtc->getCurrentTime();
      epochToClockParts(now, tz.offset_minutes, clock);

      display.setColor(DisplayDriver::GREEN);
      display.setTextSize(1);
      snprintf(tmp, sizeof(tmp), "%04u-%02u-%02u", clock.year, clock.month, clock.day);
      display.drawTextRightAlign(display.width() - 1, 18, tmp);

      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(3);
      snprintf(tmp, sizeof(tmp), "%02u:%02u", clock.hour, clock.minute);
      display.drawTextCentered(display.width() / 2, 30, tmp);

      display.setTextSize(1);
      display.setColor(DisplayDriver::GREEN);
      snprintf(tmp, sizeof(tmp), ":%02u", clock.second);
      display.drawTextCentered(display.width() / 2, 54, tmp);
    } else if (_page == HomePage::SHUTDOWN) {
      display.setColor(DisplayDriver::GREEN);
      display.setTextSize(1);
      if (_shutdown_init) {
        display.drawTextCentered(display.width() / 2, 34, "hibernating...");
      } else {
        display.drawXbm((display.width() - 32) / 2, 18, power_icon, 32, 32);
        display.drawTextCentered(display.width() / 2, 64 - 11, "hibernate:" PRESS_LABEL);
      }
    }
    return _page == HomePage::CLOCK ? 1000 : 5000;
  }

  bool handleInput(int c) override {
    if (_page == HomePage::BLUETOOTH && (c == KEY_UP || c == KEY_DOWN)) {
      _conn_cursor = (_conn_cursor == MODE_USB) ? MODE_BLE : MODE_USB;
      return true;
    }
    if (_page == HomePage::REGION && c == KEY_UP) {
      _region_idx = (_region_idx + REGION_PRESET_COUNT - 1) % REGION_PRESET_COUNT;
      return true;
    }
    if (_page == HomePage::REGION && c == KEY_DOWN) {
      _region_idx = (_region_idx + 1) % REGION_PRESET_COUNT;
      return true;
    }
    if (_page == HomePage::CLOCK && c == KEY_UP) {
      _tz_idx = (_tz_idx + TIMEZONE_OPTION_COUNT - 1) % TIMEZONE_OPTION_COUNT;
      return true;
    }
    if (_page == HomePage::CLOCK && c == KEY_DOWN) {
      _tz_idx = (_tz_idx + 1) % TIMEZONE_OPTION_COUNT;
      return true;
    }
    if (_page == HomePage::COMPOSE && c == KEY_DOWN) {
      if (_compose_presets) {
        _preset_idx = (_preset_idx + 1) % _task->getPresetMessageCount();
      } else {
        _compose_presets = true;
      }
      return true;
    }
    if (_page == HomePage::COMPOSE && c == KEY_UP && _compose_presets) {
      _preset_idx = (_preset_idx + _task->getPresetMessageCount() - 1) % _task->getPresetMessageCount();
      return true;
    }
    if (c == KEY_LEFT || c == KEY_PREV) {
      _compose_presets = false;
      _page = (_page + HomePage::Count - 1) % HomePage::Count;
      return true;
    }
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      _compose_presets = false;
      _page = (_page + 1) % HomePage::Count;
      if (_page == HomePage::RECENT) {
        _task->showAlert("Recent adverts", 800);
      }
      return true;
    }
    if ((c == KEY_ENTER || c == KEY_SELECT) && (_page == HomePage::FIRST || _page == HomePage::RECENT)) {
      _task->openMsgPreview();
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {
      _conn_mode = _conn_cursor;
      if (_conn_mode == MODE_BLE && !_task->isSerialEnabled()) {
        _task->enableSerial();
      } else if (_conn_mode == MODE_USB && _task->isSerialEnabled()) {
        _task->disableSerial();
      }
      _task->showAlert(_conn_mode == MODE_USB ? "USB mode" : "BLE mode", 800);
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::ADVERT) {
      _task->notify(UIEventType::ack);
      if (the_mesh.advert()) {
        _task->showAlert("Advert sent!", 1000);
      } else {
        _task->showAlert("Advert failed..", 1000);
      }
      return true;
    }
    if ((c == KEY_ENTER || c == KEY_SELECT) && _page == HomePage::REGION) {
      const RegionPreset& r = REGION_PRESETS[_region_idx];
      if (the_mesh.applyRadioParams(r.freq, r.bw, r.sf, r.cr, 0)) {
        saveRegionPresetIndex(_region_idx);
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%s %.3f", r.name, r.freq);
        _task->showAlert(tmp, 1000);
      } else {
        _task->showAlert("Region failed", 1000);
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::COMPOSE) {
      if (_compose_presets) {
        _task->sendPresetPublicText(_preset_idx);
        return true;
      }
      _task->openComposer();
      return true;
    }
    if ((c == KEY_LONG_ENTER || c == KEY_CONTEXT_MENU) && _page == HomePage::COMPOSE && _compose_presets) {
      _task->openPresetEditor(_preset_idx);
      return true;
    }
    if ((c == KEY_ENTER || c == KEY_SELECT) && _page == HomePage::CLOCK) {
      _task->showAlert(TIMEZONE_OPTIONS[_tz_idx].label, 800);
      return true;
    }
#if ENV_INCLUDE_GPS == 1
    if ((c == KEY_ENTER || c == KEY_SELECT) && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
#endif
#if UI_SENSORS_PAGE == 1
    if (c == KEY_ENTER && _page == HomePage::SENSORS) {
      _task->toggleGPS();
      next_sensors_refresh=0;
      return true;
    }
#endif
    if (c == KEY_ENTER && _page == HomePage::SHUTDOWN) {
      _shutdown_init = true;  // need to wait for button to be released
      return true;
    }
    return false;
  }
};

class VirtualKeyboardScreen : public UIScreen {
  enum KeyboardMode {
    MODE_SEND,
    MODE_PRESET_EDIT,
  };

  UITask* _task;
  char _text[121];
  char _pinyin[24];
  char* _candidates;
  uint8_t _len;
  uint8_t _pinyinLen;
  uint8_t _candidateOffset;
  uint8_t _row;
  uint8_t _col;
  uint8_t _presetIdx;
  uint8_t _mode;
  bool _ime;

  static const uint8_t ROWS = 4;
  static const uint8_t COLS = 11;

  static char keyAt(uint8_t row, uint8_t col) {
    static const char layout[ROWS][COLS - 1] = {
      {'1','2','3','4','5','6','7','8','9','0'},
      {'q','w','e','r','t','y','u','i','o','p'},
      {'a','s','d','f','g','h','j','k','l',' '},
      {'z','x','c','v','b','n','m','.',',','?'}
    };
    if (col >= COLS - 1) return 0;
    return layout[row][col];
  }

  const char* actionLabel(uint8_t row) {
    if (row == 0) return "DEL";
    if (row == 1) return "OK";
    if (row == 2) return _ime ? "EN" : "CN";
    return "ESC";
  }

  static const char* labelFor(char key) {
    if (key == ' ') return "_";
    static char label[2];
    label[0] = key;
    label[1] = 0;
    return label;
  }

  static uint8_t utf8CharLen(const char* str) {
    uint8_t c = (uint8_t) str[0];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
  }

  void move(int drow, int dcol) {
    _row = (_row + ROWS + drow) % ROWS;
    _col = (_col + COLS + dcol) % COLS;
  }

  void appendBytes(const char* src, uint8_t count) {
    if (_len + count >= sizeof(_text)) {
      _task->showAlert("Message full", 700);
      return;
    }
    memcpy(&_text[_len], src, count);
    _len += count;
    _text[_len] = 0;
  }

  void append(char c) {
    appendBytes(&c, 1);
  }

  void backspace() {
    if (_len == 0) return;
    do {
      _len--;
    } while (_len > 0 && (((uint8_t) _text[_len] & 0xC0) == 0x80));
    _text[_len] = 0;
  }

  uint8_t candidateCount() {
    uint8_t count = 0;
    if (!_candidates) return 0;
    for (const char* p = _candidates; *p && count < 32; count++) {
      p += utf8CharLen(p);
    }
    return count;
  }

  void refreshCandidates() {
    _candidates = (_pinyinLen > 0) ? pinyin_simple_search(_pinyin) : NULL;
    uint8_t count = candidateCount();
    if (_candidateOffset >= count) _candidateOffset = 0;
  }

  const char* candidateAt(uint8_t index) {
    if (!_candidates) return NULL;
    const char* p = _candidates;
    for (uint8_t i = 0; *p && i < index; i++) {
      p += utf8CharLen(p);
    }
    return *p ? p : NULL;
  }

  void clearPinyin() {
    _pinyinLen = 0;
    _candidateOffset = 0;
    _pinyin[0] = 0;
    _candidates = NULL;
  }

  void pinyinBackspace() {
    if (_pinyinLen == 0) return;
    _pinyin[--_pinyinLen] = 0;
    _candidateOffset = 0;
    refreshCandidates();
  }

  void addPinyin(char c) {
    if (_pinyinLen >= sizeof(_pinyin) - 1) {
      _task->showAlert("Pinyin full", 700);
      return;
    }
    _pinyin[_pinyinLen++] = c;
    _pinyin[_pinyinLen] = 0;
    _candidateOffset = 0;
    refreshCandidates();
  }

  void selectCandidate(uint8_t index) {
    const char* p = candidateAt(index);
    if (!p) {
      _task->showAlert("No match", 700);
      return;
    }
    appendBytes(p, utf8CharLen(p));
    clearPinyin();
  }

  void nextCandidate() {
    uint8_t count = candidateCount();
    if (count == 0) return;
    _candidateOffset = (_candidateOffset + 9 >= count) ? 0 : (_candidateOffset + 9);
  }

  void deleteOne() {
    if (_pinyinLen > 0) pinyinBackspace();
    else backspace();
  }

  void sendOrSelect() {
    if (_mode == MODE_PRESET_EDIT) {
      _task->savePresetMessage(_presetIdx, _text);
    } else if (_len == 0) {
      _task->showAlert("Empty message", 700);
    } else {
      _task->sendComposedPublicText(_text);
    }
  }

  void toggleIme() {
    clearPinyin();
    _ime = !_ime;
  }

  void activateLongKey() {
    if (_ime && _pinyinLen > 0 && _col < 9) {
      selectCandidate(_candidateOffset + _col);
      return;
    }
    if (_ime && _pinyinLen > 0 && _col == 9) {
      nextCandidate();
      return;
    }
    if (_col == COLS - 1 && _row == 2) {
      toggleIme();
      return;
    }
    if (_col == COLS - 1 && _row == 3) {
      toggleIme();
      return;
    }
    activateKey();
  }

  void activateKey() {
    if (_col == COLS - 1) {
      if (_row == 0) deleteOne();
      else if (_row == 1) sendOrSelect();
      else if (_row == 2) toggleIme();
      else {
        _task->gotoHomeScreen();
      }
      return;
    }

    char key = keyAt(_row, _col);
    if (_ime && key >= 'a' && key <= 'z') {
      addPinyin(key);
      return;
    }
    if (_ime && key >= '1' && key <= '9' && _pinyinLen > 0) {
      selectCandidate(_candidateOffset + (key - '1'));
      return;
    }
    if (_ime && key == '0' && _pinyinLen > 0) {
      nextCandidate();
      return;
    }
    if (_ime && key == ' ' && _pinyinLen > 0) {
      selectCandidate(_candidateOffset);
      return;
    }
    append(key);
  }

  void buildImeLine(char* dest, size_t len) {
    if (!_ime) {
      strncpy(dest, "EN", len);
      dest[len - 1] = 0;
      return;
    }

    snprintf(dest, len, "%s:", _pinyinLen ? _pinyin : "CN");
    size_t used = strlen(dest);
    uint8_t count = candidateCount();
    for (uint8_t offset = 0; offset < 9 && used + 6 < len; offset++) {
      if (count == 0) break;
      uint8_t i = _candidateOffset + offset;
      if (i >= count) break;
      const char* p = candidateAt(i);
      if (!p) break;
      uint8_t clen = utf8CharLen(p);
      memcpy(&dest[used], p, clen);
      used += clen;
      dest[used++] = ' ';
      dest[used] = 0;
    }
  }

  void renderImeLine(DisplayDriver& display) {
    display.setTextSize(1);
    if (!_ime) {
      display.setColor(DisplayDriver::GREEN);
      display.setCursor(0, 13);
      display.print("EN");
      return;
    }

    display.setColor(DisplayDriver::YELLOW);
    display.setCursor(0, 11);
    char prefix[28];
    snprintf(prefix, sizeof(prefix), "%s:", _pinyinLen ? _pinyin : "CN");
    display.print(prefix);

    uint8_t count = candidateCount();
    int x = display.getTextWidth(prefix) + 2;
    for (uint8_t offset = 0; offset < 9 && offset + _candidateOffset < count; offset++) {
      const char* p = candidateAt(_candidateOffset + offset);
      if (!p) break;
      char ch[5];
      uint8_t clen = utf8CharLen(p);
      memcpy(ch, p, clen);
      ch[clen] = 0;

      int glyph_w = display.getTextWidth(ch);
      if (x + glyph_w > display.width() - 2) break;
      display.setCursor(x, 11);
      display.print(ch);

      if (_row == 0 && _col == offset) {
        display.fillRect(x, 22, glyph_w, 2);
      }
      x += glyph_w + 3;
    }

    if (count > _candidateOffset + 9) {
      display.setCursor(display.width() - display.getTextWidth(">"), 11);
      display.print(">");
    }
  }

public:
  VirtualKeyboardScreen(UITask* task) : _task(task), _candidates(NULL), _len(0), _pinyinLen(0), _candidateOffset(0), _row(0), _col(0), _presetIdx(0), _mode(MODE_SEND), _ime(false) {
    _text[0] = 0;
    _pinyin[0] = 0;
  }

  void reset(const char* initial = NULL, bool preset_edit = false, uint8_t preset_idx = 0) {
    _len = 0;
    clearPinyin();
    _row = 0;
    _col = 0;
    _mode = preset_edit ? MODE_PRESET_EDIT : MODE_SEND;
    _presetIdx = preset_idx;
    _text[0] = 0;
    if (initial && initial[0]) {
      uint8_t len = strlen(initial);
      if (len >= sizeof(_text)) len = sizeof(_text) - 1;
      while (len > 0 && (((uint8_t) initial[len] & 0xC0) == 0x80)) len--;
      memcpy(_text, initial, len);
      _len = len;
      _text[_len] = 0;
    }
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    display.setCursor(0, 0);
    display.print(_mode == MODE_PRESET_EDIT ? "Set:" : "Msg:");

    display.setColor(DisplayDriver::LIGHT);
    display.setCursor(27, 3);
    char visible[24];
    const uint8_t max_visible = sizeof(visible) - 1;
    uint8_t start = (_len > max_visible) ? (_len - max_visible) : 0;
    while (start < _len && (((uint8_t) _text[start] & 0xC0) == 0x80)) start++;
    strncpy(visible, &_text[start], max_visible);
    visible[max_visible] = 0;
    display.print(visible);

    renderImeLine(display);

    int action_w = 25;
    int char_area_w = display.width() - action_w - 2;
    int key_w = char_area_w / (COLS - 1);
    int extra_w = char_area_w - key_w * (COLS - 1);
    int action_x = display.width() - action_w;
    int key_h = 8;
    int y0 = 29;
    for (uint8_t r = 0; r < ROWS; r++) {
      int x = 0;
      for (uint8_t c = 0; c < COLS - 1; c++) {
        int w = key_w + (c < extra_w ? 1 : 0);
        int y = y0 + r * key_h;
        bool selected = (r == _row && c == _col);
        display.setColor(selected ? DisplayDriver::LIGHT : DisplayDriver::DARK);
        if (selected) display.fillRect(x, y, w, key_h);
        display.setColor(selected ? DisplayDriver::DARK : DisplayDriver::LIGHT);
        const char* label = labelFor(keyAt(r, c));
        int tw = display.getTextWidth(label);
        display.setCursor(x + (w - tw) / 2, y + 2);
        display.print(label);
        x += w;
      }

      int y = y0 + r * key_h;
      bool selected = (r == _row && _col == COLS - 1);
      display.setColor(selected ? DisplayDriver::LIGHT : DisplayDriver::DARK);
      if (selected) display.fillRect(action_x, y, action_w, key_h);
      display.setColor(selected ? DisplayDriver::DARK : DisplayDriver::LIGHT);
      const char* label = actionLabel(r);
      int tw = display.getTextWidth(label);
      display.setCursor(action_x + (action_w - tw) / 2, y + 2);
      display.print(label);
    }
    return 250;
  }

  bool handleInput(int c) override {
    if (c == KEY_UP) {
      move(-1, 0);
      return true;
    }
    if (c == KEY_DOWN) {
      move(1, 0);
      return true;
    }
    if (c == KEY_LEFT || c == KEY_PREV) {
      move(0, -1);
      return true;
    }
    if (c == KEY_RIGHT || c == KEY_NEXT) {
      move(0, 1);
      return true;
    }
    if (c == KEY_ENTER || c == KEY_SELECT) {
      activateKey();
      return true;
    }
    if (c == KEY_LONG_ENTER || c == KEY_CONTEXT_MENU) {
      activateLongKey();
      return true;
    }
    if (c == KEY_CANCEL) {
      _task->gotoHomeScreen();
      return true;
    }
    return false;
  }
};

class MsgPreviewScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;

  struct MsgEntry {
    uint32_t timestamp;
    uint16_t origin_offset;
    uint16_t msg_offset;
    uint16_t total_len;
  };
  #define MAX_MSG_HISTORY   20
  #define MAX_MSG_HISTORY_BYTES 400
  int num_unread;
  int num_history;
  int view_from_latest;
  uint16_t history_len;
  MsgEntry history[MAX_MSG_HISTORY];
  char history_buf[MAX_MSG_HISTORY_BYTES + 1];

  static uint16_t utf8SafeLen(const char* str, uint16_t max_len) {
    uint16_t len = 0;
    while (str[len] && len < max_len) {
      uint8_t c = (uint8_t) str[len];
      uint8_t char_len = 1;
      if ((c & 0xE0) == 0xC0) char_len = 2;
      else if ((c & 0xF0) == 0xE0) char_len = 3;
      else if ((c & 0xF8) == 0xF0) char_len = 4;
      if (len + char_len > max_len) break;
      len += char_len;
    }
    return len;
  }

  void dropOldest() {
    if (num_history == 0) return;
    uint16_t remove_len = history[0].total_len;
    if (remove_len > history_len) remove_len = history_len;
    memmove(history_buf, history_buf + remove_len, history_len - remove_len);
    history_len -= remove_len;
    history_buf[history_len] = 0;
    for (int i = 1; i < num_history; i++) {
      history[i - 1] = history[i];
      history[i - 1].origin_offset -= remove_len;
      history[i - 1].msg_offset -= remove_len;
    }
    num_history--;
    if (num_unread > num_history) num_unread = num_history;
    if (view_from_latest >= num_history) view_from_latest = num_history > 0 ? num_history - 1 : 0;
  }

  const MsgEntry* currentEntry() const {
    if (num_history == 0) return NULL;
    int idx = num_history - 1 - view_from_latest;
    if (idx < 0) idx = 0;
    return &history[idx];
  }

public:
  MsgPreviewScreen(UITask* task, mesh::RTCClock* rtc) : _task(task), _rtc(rtc) {
    num_unread = 0;
    num_history = 0;
    view_from_latest = 0;
    history_len = 0;
    history_buf[0] = 0;
  }

  bool hasHistory() const {
    return num_history > 0;
  }

  void showLatest() {
    view_from_latest = 0;
  }

  void addPreview(uint8_t path_len, const char* from_name, const char* msg) {
    char origin[62];
    if (path_len == 0xFF) {
      sprintf(origin, "(D) %s:", from_name);
    } else {
      sprintf(origin, "(%d) %s:", (uint32_t) path_len, from_name);
    }

    uint16_t origin_len = utf8SafeLen(origin, sizeof(origin) - 1);
    uint16_t max_msg_len = MAX_MSG_HISTORY_BYTES - origin_len - 2;
    uint16_t msg_len = utf8SafeLen(msg, max_msg_len);
    uint16_t rec_len = origin_len + 1 + msg_len + 1;
    while ((num_history >= MAX_MSG_HISTORY || history_len + rec_len > MAX_MSG_HISTORY_BYTES) && num_history > 0) {
      dropOldest();
    }

    if (rec_len > MAX_MSG_HISTORY_BYTES) return;
    MsgEntry* p = &history[num_history++];
    p->timestamp = _rtc->getCurrentTime();
    p->origin_offset = history_len;
    memcpy(history_buf + history_len, origin, origin_len);
    history_len += origin_len;
    history_buf[history_len++] = 0;
    p->msg_offset = history_len;
    memcpy(history_buf + history_len, msg, msg_len);
    history_len += msg_len;
    history_buf[history_len++] = 0;
    history_buf[history_len] = 0;
    p->total_len = rec_len;

    if (num_unread < num_history) num_unread++;
    view_from_latest = 0;
  }

  int render(DisplayDriver& display) override {
    char tmp[16];
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    sprintf(tmp, "Unread: %d", num_unread);
    display.print(tmp);

    auto p = currentEntry();
    if (p == NULL) return 1000;

    int secs = _rtc->getCurrentTime() - p->timestamp;
    if (secs < 60) {
      sprintf(tmp, "%ds", secs);
    } else if (secs < 60*60) {
      sprintf(tmp, "%dm", secs / 60);
    } else {
      sprintf(tmp, "%dh", secs / (60*60));
    }
    display.setCursor(display.width() - display.getTextWidth(tmp) - 2, 0);
    display.print(tmp);

    display.drawRect(0, 11, display.width(), 1);  // horiz line

    display.setCursor(0, 14);
    display.setColor(DisplayDriver::YELLOW);
    display.print(history_buf + p->origin_offset);

    display.setCursor(0, 25);
    display.setColor(DisplayDriver::LIGHT);
    display.printWordWrap(history_buf + p->msg_offset, display.width());

#if AUTO_OFF_MILLIS==0 // probably e-ink
    return 10000; // 10 s
#else
    return 1000;  // next render after 1000 ms
#endif
  }

  bool handleInput(int c) override {
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      if (view_from_latest < num_history - 1) view_from_latest++;
      if (num_unread > 0) num_unread--;
      return true;
    }
    if (c == KEY_PREV || c == KEY_LEFT) {
      if (view_from_latest > 0) view_from_latest--;
      return true;
    }
    if (c == KEY_ENTER) {
      num_unread = 0;  // mark history as read
      view_from_latest = 0;
      _task->gotoHomeScreen();
      return true;
    }
    return false;
  }
};

static uint8_t utf8PresetSafeLen(const char* str, uint8_t max_len) {
  uint8_t len = 0;
  while (str[len] && len < max_len) {
    uint8_t c = (uint8_t) str[len];
    uint8_t char_len = 1;
    if ((c & 0xE0) == 0xC0) char_len = 2;
    else if ((c & 0xF0) == 0xE0) char_len = 3;
    else if ((c & 0xF8) == 0xF0) char_len = 4;
    if (len + char_len > max_len) break;
    len += char_len;
  }
  return len;
}

void UITask::loadPresetMessages() {
  static const char* DEFAULT_PRESETS[] = {
    "OK",
    "Roger",
    "On my way",
    "Need help"
  };

  for (uint8_t i = 0; i < UI_PRESET_MSG_COUNT; i++) {
    const char* text = (i < sizeof(DEFAULT_PRESETS) / sizeof(DEFAULT_PRESETS[0])) ? DEFAULT_PRESETS[i] : "";
    strncpy(_preset_msgs[i], text, UI_PRESET_MSG_BYTES);
    _preset_msgs[i][UI_PRESET_MSG_BYTES] = 0;
  }

  File f = store.openRead(PRESET_MSG_STORE_FILE);
  if (f) {
    uint8_t version = 0;
    bool ok = f.read(&version, 1) == 1 && version == PRESET_MSG_STORE_VERSION;
    if (ok) {
      for (uint8_t i = 0; i < UI_PRESET_MSG_COUNT; i++) {
        if (f.read((uint8_t*)_preset_msgs[i], UI_PRESET_MSG_BYTES) != UI_PRESET_MSG_BYTES) {
          ok = false;
          break;
        }
        _preset_msgs[i][UI_PRESET_MSG_BYTES] = 0;
      }
    }
    f.close();
    if (ok) return;
  }

  uint8_t old_blob[OLD_PRESET_MSG_BLOB_BYTES];
  uint8_t old_len = store.getBlobByKey(PRESET_MSG_STORE_KEY, sizeof(PRESET_MSG_STORE_KEY), old_blob);
  if (old_len == OLD_PRESET_MSG_BLOB_BYTES && old_blob[0] == PRESET_MSG_STORE_VERSION) {
    for (uint8_t i = 0; i < OLD_PRESET_MSG_COUNT && i < UI_PRESET_MSG_COUNT; i++) {
      memcpy(_preset_msgs[i], &old_blob[1 + i * UI_PRESET_MSG_BYTES], UI_PRESET_MSG_BYTES);
      _preset_msgs[i][UI_PRESET_MSG_BYTES] = 0;
    }
    persistPresetMessages();
  }
}

bool UITask::persistPresetMessages() {
  File f = openUiWrite(PRESET_MSG_STORE_FILE);
  if (!f) return false;

  uint8_t version = PRESET_MSG_STORE_VERSION;
  if (f.write(&version, 1) != 1) {
    f.close();
    store.removeFile(PRESET_MSG_STORE_FILE);
    return false;
  }

  bool ok = true;
  uint8_t slot[UI_PRESET_MSG_BYTES];
  for (uint8_t i = 0; i < UI_PRESET_MSG_COUNT; i++) {
    memset(slot, 0, sizeof(slot));
    uint8_t len = utf8PresetSafeLen(_preset_msgs[i], UI_PRESET_MSG_BYTES);
    memcpy(slot, _preset_msgs[i], len);
    if (f.write(slot, sizeof(slot)) != sizeof(slot)) {
      ok = false;
      break;
    }
  }
  f.close();
  if (!ok) store.removeFile(PRESET_MSG_STORE_FILE);
  return ok;
}

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _auto_off = millis() + AUTO_OFF_MILLIS;

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if UI_HAS_JOYSTICK
  joystick_up.begin();
  joystick_down.begin();
  joystick_left.begin();
  joystick_right.begin();
  back_btn.begin();
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  _node_prefs = node_prefs;
  loadPresetMessages();

  if (_display != NULL) {
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(false);
  _node_prefs->buzzer_quiet = 0;
  buzzer.startup();
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  ui_started_at = millis();
  _alert_expiry = 0;

  splash = new SplashScreen(this);
  home = new HomeScreen(this, &rtc_clock, sensors, node_prefs);
  msg_preview = new MsgPreviewScreen(this, &rtc_clock);
  composer = new VirtualKeyboardScreen(this);
  setCurrScreen(splash);
}

void UITask::openMsgPreview() {
  MsgPreviewScreen* preview = (MsgPreviewScreen *) msg_preview;
  if (!preview->hasHistory()) {
    showAlert("No messages", 800);
    return;
  }
  preview->showLatest();
  setCurrScreen(msg_preview);
}

void UITask::openComposer() {
  ((VirtualKeyboardScreen *) composer)->reset();
  setCurrScreen(composer);
}

void UITask::openPresetEditor(uint8_t idx) {
  if (idx >= UI_PRESET_MSG_COUNT) idx = 0;
  ((VirtualKeyboardScreen *) composer)->reset(_preset_msgs[idx], true, idx);
  setCurrScreen(composer);
}

void UITask::sendComposedPublicText(const char* text) {
  if (the_mesh.sendPublicText(text)) {
    notify(UIEventType::ack);
    showAlert("Sent to Public", 1000);
  } else {
    showAlert("Send failed", 1000);
  }
  gotoHomeScreen();
}

void UITask::sendPresetPublicText(uint8_t idx) {
  if (idx >= UI_PRESET_MSG_COUNT) idx = 0;
  if (_preset_msgs[idx][0] == 0) {
    showAlert("Hold to edit", 900);
    return;
  }
  sendComposedPublicText(_preset_msgs[idx]);
}

void UITask::savePresetMessage(uint8_t idx, const char* text) {
  if (idx >= UI_PRESET_MSG_COUNT) idx = 0;
  uint8_t len = utf8PresetSafeLen(text, UI_PRESET_MSG_BYTES);
  memcpy(_preset_msgs[idx], text, len);
  _preset_msgs[idx][len] = 0;
  if (persistPresetMessages()) {
    notify(UIEventType::ack);
    showAlert("Preset saved", 1000);
  } else {
    showAlert("Save failed", 1000);
  }
  gotoHomeScreen();
}

const char* UITask::getPresetMessage(uint8_t idx) const {
  if (idx >= UI_PRESET_MSG_COUNT) idx = 0;
  return _preset_msgs[idx];
}

void UITask::showAlert(const char* text, int duration_millis) {
  strcpy(_alert, text);
  _alert_expiry = millis() + duration_millis;
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
switch(t){
  case UIEventType::contactMessage:
    buzzer.play("Msg:d=16,o=6,b=180:c,p,c,p,c");
    break;
  case UIEventType::channelMessage:
    buzzer.play("Msg:d=16,o=6,b=180:c,p,c,p,c");
    break;
  case UIEventType::ack:
    buzzer.play("Key:d=32,o=7,b=160:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif

#ifdef PIN_VIBRATION
  // Trigger vibration for all UI events except none
  if (t != UIEventType::none) {
    vibration.trigger();
  }
#endif
}


void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
  if (msgcount == 0) {
    gotoHomeScreen();
  }
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  _msgcount = msgcount;

  ((MsgPreviewScreen *) msg_preview)->addPreview(path_len, from_name, text);
  setCurrScreen(msg_preview);

  if (_display != NULL) {
    if (!_display->isOn() && !hasConnection()) {
      _display->turnOn();
    }
    if (_display->isOn()) {
    _auto_off = millis() + AUTO_OFF_MILLIS;  // extend the auto-off timer
    _next_refresh = 100;  // trigger refresh
    }
  }
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

void UITask::setCurrScreen(UIScreen* c) {
  curr = c;
  _next_refresh = 100;
}

/*
  hardware-agnostic pre-shutdown activity should be done here
*/
void UITask::shutdown(bool restart){

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - 2500) < buzzer_timer)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    _board->reboot();
  } else {
    _display->turnOff();
    radio_driver.powerOff();
    _board->powerOff();
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

#ifdef CASTLEBOY_GAME
void UITask::runGame() {
  const bool a_pressed = digitalRead(PIN_BACK_BTN) == LOW;
  const bool b_pressed = digitalRead(JOYSTICK_PRESS) == LOW;

  if (a_pressed && b_pressed) {
    if (_game_exit_started == 0) {
      _game_exit_started = millis();
    } else if (millis() - _game_exit_started >= 1200) {
      _game_active = false;
      _game_exit_started = 0;
      noTone(PIN_BUZZER);
      gotoHomeScreen();
      _next_refresh = 0;
      return;
    }
  } else {
    _game_exit_started = 0;
  }

  if (millis() < _game_next_frame) return;
  _game_next_frame = millis() + 16;

  uint8_t buttons = 0;
  if (digitalRead(JOYSTICK_LEFT) == LOW)  buttons |= CastleBoyApp::Left;
  if (digitalRead(JOYSTICK_RIGHT) == LOW) buttons |= CastleBoyApp::Right;
  if (digitalRead(JOYSTICK_UP) == LOW)    buttons |= CastleBoyApp::Up;
  if (digitalRead(JOYSTICK_DOWN) == LOW)  buttons |= CastleBoyApp::Down;
  if (a_pressed)                          buttons |= CastleBoyApp::A;
  if (b_pressed)                          buttons |= CastleBoyApp::B;

  CastleBoyApp::step(buttons);
  static_cast<SSD1306Display*>(_display)->drawNativeBuffer(CastleBoyApp::buffer(), 128 * 64 / 8);
  _display->endFrame();
  _auto_off = millis() + AUTO_OFF_MILLIS;
}
#endif

void UITask::loop() {
#ifdef CASTLEBOY_GAME
  if (_game_active) {
    userLedHandler();
    runGame();
    return;
  }
#endif

  int c = 0;
#if UI_HAS_JOYSTICK
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    int long_c = checkDisplayOn(KEY_LONG_ENTER);
    bool long_consumed = false;
    if (long_c != 0 && curr && millis() - ui_started_at >= 8000) {
      long_consumed = curr->handleInput(long_c);
      if (long_consumed) {
#ifdef PIN_BUZZER
        if (!buzzer.isPlaying()) {
          buzzer.play("Key:d=32,o=7,b=160:c");
        }
#endif
        _auto_off = millis() + AUTO_OFF_MILLIS;
        _next_refresh = 100;
      }
    }
    if (long_consumed) {
      c = 0;
    } else {
#ifdef CASTLEBOY_GAME
    if (millis() - ui_started_at >= 8000 && _display != NULL) {
      _display->turnOn();
      CastleBoyApp::begin();
      _game_active = true;
      _game_next_frame = 0;
      _auto_off = millis() + AUTO_OFF_MILLIS;
      c = 0;
    } else
#endif
    c = handleLongPress(KEY_LONG_ENTER);
    }
  }
  ev = joystick_up.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_UP);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_UP);
  }
  ev = joystick_down.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_DOWN);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_DOWN);
  }
  ev = joystick_left.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_LEFT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_LEFT);
  }
  ev = joystick_right.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_RIGHT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_RIGHT);
  }
  ev = back_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_CANCEL);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_CANCEL);
  } else
  if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#elif defined(PIN_USER_BTN)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#endif
#if defined(PIN_USER_BTN_ANA)
  if (abs(millis() - _analogue_pin_read_millis) > 10) {
    int ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    }
    _analogue_pin_read_millis = millis();
  }
#endif
#if defined(BACKLIGHT_BTN)
  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

  if (c != 0 && curr) {
    curr->handleInput(c);
#ifdef PIN_BUZZER
    if (!buzzer.isPlaying()) {
      buzzer.play("Key:d=32,o=7,b=160:c");
    }
#endif
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 100;  // trigger refresh
  }

  userLedHandler();

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  if (curr) curr->poll();

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh && curr) {
      _display->startFrame();
      int delay_millis = curr->render(*_display);
      if (millis() < _alert_expiry) {  // render alert popup
        _display->setTextSize(1);
        int y = _display->height() / 3;
        int p = _display->height() / 32;
        _display->setColor(DisplayDriver::DARK);
        _display->fillRect(p, y, _display->width() - p*2, y);
        _display->setColor(DisplayDriver::LIGHT);  // draw box border
        _display->drawRect(p, y, _display->width() - p*2, y);
        _display->drawTextCentered(_display->width() / 2, y + p*3, _alert);
        _next_refresh = _alert_expiry;   // will need refresh when alert is dismissed
      } else {
        _next_refresh = millis() + delay_millis;
      }
      _display->endFrame();
    }
#if AUTO_OFF_MILLIS > 0
#ifdef KEEP_DISPLAY_ON_USB
    // Opt-in: refresh the auto-off deadline while externally powered, so the
    // timer counts from the moment external power is removed. Off by default
    // because OLED panels burn in quickly; only enable for LCD targets or
    // where the display is replaceable.
    if (board.isExternalPowered()) {
      _auto_off = millis() + AUTO_OFF_MILLIS;
    }
#endif
    if (millis() > _auto_off) {
      _display->turnOff();
    }
#endif
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

#ifdef AUTO_SHUTDOWN_MILLIVOLTS
  if (millis() > next_batt_chck) {
    uint16_t milliVolts = getBattMilliVolts();
    if (milliVolts > 0 && milliVolts < AUTO_SHUTDOWN_MILLIVOLTS) {
      if(!board.isExternalPowered()) {
        if (_display != NULL) {
          _display->startFrame();
          _display->setTextSize(2);
          _display->setColor(DisplayDriver::RED);
          _display->drawTextCentered(_display->width() / 2, 20, "Low Battery.");
          _display->drawTextCentered(_display->width() / 2, 40, "Shutting Down!");
          _display->endFrame();
          if (_display->isEink() == false) { delay(3000); }
        }
        shutdown();
      }
    }
    next_batt_chck = millis() + 8000;
  }
#endif
}

int UITask::checkDisplayOn(int c) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();   // turn display on and consume event
      c = 0;
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 0;  // trigger refresh
  }
  return c;
}

int UITask::handleLongPress(int c) {
  if (millis() - ui_started_at < 8000) {   // long press in first 8 seconds since startup -> CLI/rescue
    the_mesh.enterCLIRescue();
    c = 0;   // consume event
  }
  return c;
}

int UITask::handleDoubleClick(int c) {
  MESH_DEBUG_PRINTLN("UITask: double-click triggered");
  checkDisplayOn(c);
  return c;
}

int UITask::handleTripleClick(int c) {
  MESH_DEBUG_PRINTLN("UITask: triple click triggered");
  checkDisplayOn(c);
  toggleBuzzer();
  c = 0;
  return c;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  }
  return false;
}

void UITask::toggleGPS() {
  bool handled = false;
  if (_sensors != NULL) {
    // toggle GPS on/off
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        if (strcmp(_sensors->getSettingValue(i), "1") == 0) {
          _sensors->setSettingValue("gps", "0");
          _node_prefs->gps_enabled = 0;
          notify(UIEventType::ack);
        } else {
          _sensors->setSettingValue("gps", "1");
          _node_prefs->gps_enabled = 1;
          notify(UIEventType::ack);
        }
        the_mesh.savePrefs();
        showAlert(_node_prefs->gps_enabled ? "GPS: Enabled" : "GPS: Disabled", 800);
        _next_refresh = 0;
        handled = true;
        break;
      }
    }
  }
  if (!handled) {
    _node_prefs->gps_enabled = _node_prefs->gps_enabled ? 0 : 1;
    the_mesh.savePrefs();
    showAlert(_node_prefs->gps_enabled ? "GPS: Enabled" : "GPS: Disabled", 800);
    _next_refresh = 0;
  }
}

void UITask::toggleBuzzer() {
    // Toggle buzzer quiet mode
  #ifdef PIN_BUZZER
    if (buzzer.isQuiet()) {
      buzzer.quiet(false);
      notify(UIEventType::ack);
    } else {
      buzzer.quiet(true);
    }
    _node_prefs->buzzer_quiet = buzzer.isQuiet();
    the_mesh.savePrefs();
    showAlert(buzzer.isQuiet() ? "Buzzer: OFF" : "Buzzer: ON", 800);
    _next_refresh = 0;  // trigger refresh
  #endif
}
