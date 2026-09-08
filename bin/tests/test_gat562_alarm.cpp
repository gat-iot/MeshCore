#include "../../examples/companion_radio/ui-new/GAT562Alarm.h"
#include <assert.h>
#include <stdio.h>
using namespace GAT562Alarm;
int main() {
  Settings s;
  // 2024-01-01 Monday, 00:00 UTC / 08:00 UTC+8.
  const uint32_t monday = 1704067200UL;
  s.entries[0].hour = 8; s.entries[0].enabled = 1;
  assert(due(s, monday, false) == 0);
  assert(due(s, monday-60, true) == 0);
  assert(due(s, monday, true) == 1);
  assert(due(s, monday+59, true) == 0);
  assert(due(s, monday+86400, true) == 1);
  assert(due(s, monday, true) == 0); // Backward correction never repeats a fired day.
  uint8_t bytes[STORAGE_SIZE]; encode(s, bytes);
  Settings restored;
  assert(decode(bytes, sizeof(bytes), restored));
  assert(due(restored, monday+86400, true) == 0); // Reboot in same minute.
  bytes[5] ^= 1;
  assert(!decode(bytes, sizeof(bytes), restored));
  assert(!decode(bytes, sizeof(bytes)-1, restored));
  s = Settings();
  for (auto& e : s.entries) { e.hour = 8; e.enabled = 1; }
  s.entries[0].repeat = 0; s.entries[1].repeat = 2;
  assert(due(s, monday, true) == 7);
  assert(s.entries[0].enabled == 0);
  assert(due(s, monday+5*86400, true) == 4); // Saturday, only daily.
  assert(due(s, monday+6*86400, true) == 4); // Sunday.
  assert(due(s, monday+7*86400, true) == 6); // Next Monday.
  s = Settings(); s.timezone = 0; // UTC-12, Sunday locally at UTC Monday noon minus 1s.
  s.entries[0].enabled = 1; s.entries[0].hour = 0; s.entries[0].repeat = 2;
  assert(due(s, monday+12*3600-1, true) == 0);
  assert(due(s, monday+12*3600, true) == 1);
  s = Settings(); s.timezone = 26;
  s.entries[0].enabled = 1; s.entries[0].hour = 0;
  assert(due(s, monday+10*3600, true) == 1); // UTC+14, next local day.
  s.entries[0].hour = 24; assert(!valid(s));
  assert(!elapsed(0xffffff00UL, 0x100UL));
  assert(elapsed(0x100UL, 0x100UL));
  assert(elapsed(0x200UL, 0x100UL));
  puts("GAT562 alarm tests passed");
}
