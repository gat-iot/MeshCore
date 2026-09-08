#pragma once

#ifdef GAT562_CH_UI
#include "GAT562CHUI.h"
namespace GAT562AlarmText = GAT562CHUI;
#else
namespace GAT562AlarmText {
static const char ALARM[] = "Alarm";
static const char ALARM_SAVED[] = "Alarm saved";
static const char SAVE_FAILED[] = "Save failed";
static const char ALARM_OPEN[] = "Enter: set alarms";
static const char ALARM_READY[] = "Time synced";
static const char ALARM_WAIT[] = "Await APP/GPS sync";
static const char ALARM_SAVE[] = "Enter:save Back:exit";
static const char ALARM_STOP[] = "Enter/Back: stop";
static const char ALARM_SNOOZE[] = "Up: snooze 5 min";
static const char ALARM_ENABLED[] = "Enabled";
static const char ALARM_DISABLED[] = "Disabled";
static const char* const ALARM_REPEAT[] = { "Once", "Daily", "Weekdays" };
}
#endif
