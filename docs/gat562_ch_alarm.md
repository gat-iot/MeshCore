# GAT562 Alarm

Available in Family, T9, Family CH and T9 CH. All four builds enable `GAT562_ALARM` and share the same scheduling, storage and input logic. `GAT562_CH_UI` selects Chinese alarm text; non-CH builds use English text without Chinese fonts or the pinyin library. Alarm settings use the same storage format across language variants.

## Controls

- Open the alarm page immediately after the clock page; press Enter.
- Up/down selects alarm number, hour, minute, repeat mode, or enabled state.
- Left/right changes the selected value. There are three independent alarms.
- Repeat modes are once, daily, and weekdays (Monday through Friday).
- Enter saves all three alarms. Back cancels unsaved edits.
- During ringing, Enter/Back stops; Up snoozes for five minutes. A T9 character also stops ringing without opening the composer or entering text.
- Ringing stops automatically after 60 seconds. Simultaneous alarms share one notification.

## Time And Power

The timezone is shared with the clock page and saved with alarm settings. Default: UTC+8. Change it with up/down on the clock page.

After every reboot, alarms wait for a successful APP time-setting command or GPS time synchronization. The firmware's fallback date and cached contact timestamps are not trusted clock sources. The alarm page indicates synchronization status. A connection alone is not proof of synchronization.

Alarms work with the display off while the device remains powered on. They cannot wake a fully shut-down device. Snooze uses elapsed uptime, is not affected by timezone changes, and is cancelled by reboot or saving new alarm settings. Clock jumps do not replay missed minutes. Dates already fired are persisted to prevent repeating an alarm after restart or a backward clock adjustment; explicitly editing an entry rearms it.

Alarm sound temporarily overrides the message mute setting, then restores it. Games pause during ringing. Incoming messages remain in message history; alarm sound has priority over message tones. Mesh, BLE and USB servicing continue in the main loop.

## Storage And Verification

Settings use the separate `/ui_alarms` file with version, explicit byte serialization and CRC validation. Existing node preferences and message presets are unchanged. Invalid alarm data loads as disabled defaults. Saving errors are reported on screen.

The firmware workflow runs `bin/tests/test_gat562_alarm.cpp`, builds all four Family/T9 and CH/non-CH variants, and checks USB, keyboard and language/alarm symbols before publishing UF2 and OTA ZIP artifacts.

Hardware acceptance: synchronize time, schedule two minutes ahead, verify screen-off wake/ringing, stop, snooze, simultaneous alarms, incoming messages during ringing, USB COM, BLE pairing, T9 first-key handling, and settings after reboot. Physical timing, audio and USB enumeration still require device testing.
