#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <GAT562MeshTrackerProBoard.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#ifdef GAT562_T9_KEYBOARD
  #include <GAT562T9Keyboard.h>
#endif

#ifdef DISPLAY_CLASS
  #include <helpers/ui/SSD1306Display.h>
  extern DISPLAY_CLASS display;
  #include <helpers/ui/MomentaryButton.h>
  extern MomentaryButton user_btn;
  extern MomentaryButton joystick_up;
  extern MomentaryButton joystick_down;
  extern MomentaryButton joystick_left;
  extern MomentaryButton joystick_right;
  extern MomentaryButton back_btn;
#ifdef GAT562_T9_KEYBOARD
  extern MomentaryButton t9_del_btn;
  extern GAT562T9Keyboard t9_keyboard;
#endif
#endif

extern GAT562MeshTrackerProBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

bool radio_init();
mesh::LocalIdentity radio_new_identity();
