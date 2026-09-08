#pragma once
#include <stdint.h>
#include <stddef.h>

namespace GAT562Alarm {
static const uint8_t COUNT = 3;
static const size_t STORAGE_SIZE = 30;
struct Entry {
  uint8_t hour = 7, minute = 0, repeat = 1, enabled = 0;
  uint32_t firedDay = 0;
};
struct Settings {
  uint8_t timezone = 20;
  Entry entries[COUNT];
};
inline bool valid(const Settings& s) {
  if (s.timezone > 26) return false;
  for (const auto& e : s.entries)
    if (e.hour > 23 || e.minute > 59 || e.repeat > 2 || e.enabled > 1) return false;
  return true;
}
inline void encode(const Settings& s, uint8_t* b) {
  b[0] = 'A'; b[1] = 'L'; b[2] = 1; b[3] = s.timezone;
  for (uint8_t i = 0; i < COUNT; ++i) {
    const Entry& e = s.entries[i];
    uint8_t* p = b + 4 + i * 8;
    p[0] = e.hour; p[1] = e.minute; p[2] = e.repeat; p[3] = e.enabled;
    for (int j = 0; j < 4; ++j) p[4+j] = e.firedDay >> (j*8);
  }
  uint16_t crc = 0xffff;
  for (size_t i = 0; i < STORAGE_SIZE-2; ++i) {
    crc ^= uint16_t(b[i]) << 8;
    for (int j = 0; j < 8; ++j) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  b[28] = crc; b[29] = crc >> 8;
}
inline bool decode(const uint8_t* b, size_t n, Settings& out) {
  if (n != STORAGE_SIZE || b[0] != 'A' || b[1] != 'L' || b[2] != 1) return false;
  Settings s; s.timezone = b[3];
  for (uint8_t i = 0; i < COUNT; ++i) {
    Entry& e = s.entries[i]; const uint8_t* p = b + 4 + i*8;
    e.hour = p[0]; e.minute = p[1]; e.repeat = p[2]; e.enabled = p[3];
    e.firedDay = 0;
    for (int j = 0; j < 4; ++j) e.firedDay |= uint32_t(p[4+j]) << (j*8);
  }
  if (!valid(s)) return false;
  uint8_t check[STORAGE_SIZE]; encode(s, check);
  if (check[28] != b[28] || check[29] != b[29]) return false;
  out = s; return true;
}
inline uint8_t due(Settings& s, uint32_t utc, bool synchronized) {
  if (!synchronized || utc < 1704067200UL || !valid(s)) return 0;
  const int64_t local = int64_t(utc) + (int(s.timezone)-12)*3600;
  const uint32_t day = local / 86400;
  const uint16_t minute = (local % 86400) / 60;
  const uint8_t weekday = (day + 4) % 7; // Sunday = 0; epoch began Thursday.
  uint8_t mask = 0;
  for (uint8_t i = 0; i < COUNT; ++i) {
    Entry& e = s.entries[i];
    if (!e.enabled || e.firedDay >= day || minute != e.hour*60+e.minute) continue;
    if (e.repeat == 2 && (weekday == 0 || weekday == 6)) continue;
    e.firedDay = day;
    if (e.repeat == 0) e.enabled = 0;
    mask |= 1 << i;
  }
  return mask;
}
inline bool elapsed(uint32_t now, uint32_t deadline) {
  return int32_t(now - deadline) >= 0;
}
}
