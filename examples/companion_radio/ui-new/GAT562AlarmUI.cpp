#ifdef GAT562_ALARM
#include "UITask.h"
#include "GAT562AlarmText.h"
#include "../MyMesh.h"
#include "target.h"

extern DataStore store;
#ifndef AUTO_OFF_MILLIS
#define AUTO_OFF_MILLIS 15000
#endif
static const char* ALARM_FILE = "/ui_alarms";

void UITask::loadAlarms() {
  File f = store.openRead(ALARM_FILE);
  if (!f) return;
  uint8_t bytes[GAT562Alarm::STORAGE_SIZE];
  const size_t n = f.read(bytes, sizeof(bytes));
  const bool exact = f.size() == sizeof(bytes);
  f.close();
  if (exact) GAT562Alarm::decode(bytes, n, _alarms);
}

bool UITask::saveAlarms(const GAT562Alarm::Settings& settings) {
  if (!GAT562Alarm::valid(settings)) return false;
  uint8_t bytes[GAT562Alarm::STORAGE_SIZE];
  GAT562Alarm::encode(settings, bytes);
  FILESYSTEM* fs = store.getPrimaryFS();
  fs->remove(ALARM_FILE);
  File f = fs->open(ALARM_FILE, FILE_O_WRITE);
  if (!f) return false;
  const bool ok = f.write(bytes, sizeof(bytes)) == sizeof(bytes);
  f.close();
  if (!ok) store.removeFile(ALARM_FILE);
  return ok;
}

bool UITask::setAlarmTimezone(uint8_t index) {
  auto next = _alarms;
  next.timezone = index;
  if (!saveAlarms(next)) {
    showAlert(GAT562AlarmText::SAVE_FAILED, 1500);
    return false;
  }
  _alarms = next;
  return true;
}

void UITask::openAlarms() {
  _alarm_draft = _alarms;
  _alarm_editing = true;
  _alarm_field = 0;
  _alarm_refresh = millis();
}

void UITask::stopAlarm(bool snooze) {
  if (snooze) {
    _alarm_snoozed |= _alarm_ringing;
    _alarm_snooze_at = millis() + 300000UL;
  }
  _alarm_ringing = 0;
#ifdef PIN_BUZZER
  rtttl::stop();
  noTone(PIN_BUZZER);
  buzzer.quiet(_alarm_quiet);
#endif
#ifdef GAT562_T9_KEYBOARD
  t9_keyboard.resetInputState();
#endif
  _auto_off = millis() + AUTO_OFF_MILLIS;
  _next_refresh = 0;
  _alarm_refresh = millis();
}

bool UITask::pollAlarms() {
  const uint32_t now = millis();
  uint8_t due = 0;
  if (GAT562Alarm::elapsed(now, _alarm_check)) {
    _alarm_check = now + 500;
    bool needs_clock = _alarm_ringing || _alarm_snoozed;
    for (const auto& e : _alarms.entries) needs_clock |= e.enabled != 0;
    if (needs_clock && rtc_clock.isTimeSynchronized()) {
      _alarm_epoch = rtc_clock.getCurrentTime();
      due = GAT562Alarm::due(_alarms, _alarm_epoch, true);
    }
  }
  if (due) {
    // Persist one-shot disable and fired dates, never write on ordinary clock ticks.
    if (!saveAlarms(_alarms)) showAlert(GAT562AlarmText::SAVE_FAILED, 3000);
    for (uint8_t i = 0; i < GAT562Alarm::COUNT; ++i) {
      _alarm_draft.entries[i].firedDay = _alarms.entries[i].firedDay;
      if (due & (1 << i)) _alarm_draft.entries[i].enabled = _alarms.entries[i].enabled;
    }
  }
  if (_alarm_snoozed && GAT562Alarm::elapsed(now, _alarm_snooze_at)) {
    due |= _alarm_snoozed;
    _alarm_snoozed = 0;
  }
  if (due) {
    if (!_alarm_ringing) {
#ifdef PIN_BUZZER
      _alarm_quiet = buzzer.isQuiet();
      rtttl::stop();
      noTone(PIN_BUZZER);
      buzzer.quiet(false);
#endif
      _alarm_started = now;
      _alarm_armed = false;
#ifdef GAT562_T9_KEYBOARD
      t9_keyboard.resetInputState();
#endif
    }
    _alarm_ringing |= due;
    _alarm_refresh = now;
  }
  if (!_alarm_ringing && !_alarm_editing) return false;

  // Own input while editing/ringing so T9 cannot open the message composer.
  int key = 0;
  const int enter = user_btn.check();
  const int back = back_btn.check();
  const int up = joystick_up.check();
  const int down = joystick_down.check();
  const int left = joystick_left.check();
  const int right = joystick_right.check();
  if (enter == BUTTON_EVENT_CLICK) key = KEY_ENTER;
  if (back == BUTTON_EVENT_CLICK) key = KEY_CANCEL;
  if (up == BUTTON_EVENT_CLICK) key = KEY_UP;
  if (down == BUTTON_EVENT_CLICK) key = KEY_DOWN;
  if (left == BUTTON_EVENT_CLICK) key = KEY_LEFT;
  if (right == BUTTON_EVENT_CLICK) key = KEY_RIGHT;
#ifdef GAT562_T9_KEYBOARD
  char ch = 0;
  if (t9_keyboard.poll(ch) && _alarm_ringing) key = KEY_CANCEL;
  if (t9_del_btn.check() == BUTTON_EVENT_CLICK) key = KEY_CANCEL;
#endif

  if (_alarm_ringing) {
    const bool released = digitalRead(JOYSTICK_PRESS) != LOW && digitalRead(PIN_BACK_BTN) != LOW &&
      digitalRead(JOYSTICK_UP) != LOW && digitalRead(JOYSTICK_DOWN) != LOW &&
      digitalRead(JOYSTICK_LEFT) != LOW && digitalRead(JOYSTICK_RIGHT) != LOW;
    if (!_alarm_armed) {
      key = 0;
      if (now - _alarm_started > 300 && released) _alarm_armed = true;
    }
    if (key == KEY_UP || key == KEY_ENTER || key == KEY_CANCEL || now - _alarm_started >= 60000UL) {
      stopAlarm(key == KEY_UP);
      return true;
    }
#ifdef PIN_BUZZER
    if (!buzzer.isPlaying()) buzzer.play("Alarm:d=8,o=6,b=140:c,e,g,p,c,e,g,4p");
    buzzer.loop();
#endif
    if (_display && !_display->isOn()) _display->turnOn();
    _auto_off = now + AUTO_OFF_MILLIS;
  } else {
    if (key) key = checkDisplayOn(key);
    if (key == KEY_CANCEL) {
      _alarm_editing = false;
      _next_refresh = 0;
      return true;
    }
    if (key == KEY_ENTER) {
      if (saveAlarms(_alarm_draft)) {
        _alarms = _alarm_draft;
        _alarm_snoozed = 0;
        _alarm_editing = false;
        showAlert(GAT562AlarmText::ALARM_SAVED, 1000);
      } else {
        // Return to the normal alert renderer without applying unsaved settings.
        _alarm_editing = false;
        showAlert(GAT562AlarmText::SAVE_FAILED, 2000);
      }
      _next_refresh = 0;
      return true;
    }
    if (key == KEY_UP) _alarm_field = (_alarm_field + 4) % 5;
    if (key == KEY_DOWN) _alarm_field = (_alarm_field + 1) % 5;
    if (key == KEY_LEFT || key == KEY_RIGHT) {
      const int delta = key == KEY_RIGHT ? 1 : -1;
      auto& e = _alarm_draft.entries[_alarm_slot];
      if (_alarm_field == 0) _alarm_slot = (_alarm_slot + 3 + delta) % 3;
      if (_alarm_field == 1) e.hour = (e.hour + 24 + delta) % 24;
      if (_alarm_field == 2) e.minute = (e.minute + 60 + delta) % 60;
      if (_alarm_field == 3) e.repeat = (e.repeat + 3 + delta) % 3;
      if (_alarm_field == 4) e.enabled = !e.enabled;
      if (_alarm_field != 0) e.firedDay = 0;
    }
#ifdef PIN_BUZZER
    buzzer.loop();
#endif
    if (key) _alarm_refresh = now;
  }
  userLedHandler();
#ifdef PIN_VIBRATION
  vibration.loop();
#endif
  if (_display && _display->isOn() && GAT562Alarm::elapsed(now, _alarm_refresh)) {
    auto& d = *_display;
    char text[48];
    d.startFrame();
    d.setTextSize(1);
    d.setColor(UIColor::primary_txt);
    if (_alarm_ringing) {
      snprintf(text, sizeof(text), "%s %s%s%s", GAT562AlarmText::ALARM,
        _alarm_ringing & 1 ? "1 " : "", _alarm_ringing & 2 ? "2 " : "", _alarm_ringing & 4 ? "3" : "");
      d.drawTextCentered(64, 2, text);
      const uint32_t local = _alarm_epoch + (int(_alarms.timezone)-12)*3600;
      snprintf(text, sizeof(text), "%02u:%02u", unsigned(local / 3600 % 24), unsigned(local / 60 % 60));
      d.setTextSize(2); d.drawTextCentered(64, 18, text); d.setTextSize(1);
      d.drawTextCentered(64, 40, GAT562AlarmText::ALARM_STOP);
      d.drawTextCentered(64, 53, GAT562AlarmText::ALARM_SNOOZE);
    } else {
      const auto& e = _alarm_draft.entries[_alarm_slot];
      snprintf(text, sizeof(text), "%c%s %u/3", _alarm_field == 0 ? '>' : ' ', GAT562AlarmText::ALARM, _alarm_slot+1);
      d.setCursor(0, 0); d.print(text);
      snprintf(text, sizeof(text), "%c%02u : %c%02u", _alarm_field == 1 ? '>' : ' ', e.hour, _alarm_field == 2 ? '>' : ' ', e.minute);
      d.setCursor(0, 14); d.print(text);
      snprintf(text, sizeof(text), "%c%s", _alarm_field == 3 ? '>' : ' ', GAT562AlarmText::ALARM_REPEAT[e.repeat]);
      d.setCursor(0, 27); d.print(text);
      snprintf(text, sizeof(text), "%c%s", _alarm_field == 4 ? '>' : ' ', e.enabled ? GAT562AlarmText::ALARM_ENABLED : GAT562AlarmText::ALARM_DISABLED);
      d.setCursor(0, 40); d.print(text);
      d.drawTextCentered(64, 53, GAT562AlarmText::ALARM_SAVE);
    }
    d.endFrame();
    _alarm_refresh = now + 250;
  }
  if (!_alarm_ringing && _display && _display->isOn() && GAT562Alarm::elapsed(now, _auto_off)) _display->turnOff();
  return true;
}
#endif
