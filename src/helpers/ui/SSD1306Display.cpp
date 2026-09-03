#include "SSD1306Display.h"
#include "utf8_10x10.h"

bool SSD1306Display::i2c_probe(TwoWire& wire, uint8_t addr) {
  wire.beginTransmission(addr);
  uint8_t error = wire.endTransmission();
  return (error == 0);
}

// Color scheme
ColorVal UIColor::window_bkg = SSD1306_BLACK;
ColorVal UIColor::title_bkg = SSD1306_BLACK;
ColorVal UIColor::title_txt = SSD1306_WHITE;
ColorVal UIColor::primary_txt = SSD1306_WHITE;
ColorVal UIColor::secondary_txt = SSD1306_WHITE;
ColorVal UIColor::warning_txt = SSD1306_WHITE;
ColorVal UIColor::popup_bkg = SSD1306_BLACK;
ColorVal UIColor::popup_txt = SSD1306_WHITE;
ColorVal UIColor::corp_blue = SSD1306_WHITE;

bool SSD1306Display::begin() {
  if (!_isOn) {
    if (_peripher_power) _peripher_power->claim();
    _isOn = true;
  }
  #ifdef DISPLAY_ROTATION
  display.setRotation(DISPLAY_ROTATION);
  #endif
  return display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS, true, false) && i2c_probe(Wire, DISPLAY_ADDRESS);
}

void SSD1306Display::turnOn() {
  if (!_isOn) {
    if (_peripher_power) _peripher_power->claim();
    _isOn = true;  // set before begin() to prevent double claim
    if (_peripher_power) begin();  // re-init display after power was cut
  }
  display.ssd1306_command(SSD1306_DISPLAYON);
}

void SSD1306Display::turnOff() {
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  if (_isOn) {
    if (_peripher_power) {
#if PIN_OLED_RESET >= 0
      digitalWrite(PIN_OLED_RESET, LOW);
#endif
      _peripher_power->release();
    }
    _isOn = false;
  }
}

void SSD1306Display::clear() {
  display.clearDisplay();
  display.display();
}

void SSD1306Display::startFrame(ColorVal bkg) {
  display.clearDisplay();  // TODO: apply 'bkg'
  _color = SSD1306_WHITE;
  _cursorX = 0;
  _cursorY = 0;
  _textSize = 1;
  display.setTextColor(_color);
  display.setTextSize(1);
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
}

void SSD1306Display::setTextSize(int sz) {
  _textSize = sz;
  display.setTextSize(sz);
}

void SSD1306Display::setColor(ColorVal c) {
  _color = c;
  display.setTextColor(_color);
}

void SSD1306Display::setCursor(int x, int y) {
  _cursorX = x;
  _cursorY = y;
  display.setCursor(x, y);
}

uint16_t SSD1306Display::nextCodepoint(const char*& str) {
  uint8_t c = (uint8_t) *str++;
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0 && (*str & 0xC0) == 0x80) {
    uint16_t cp = ((c & 0x1F) << 6) | (*str++ & 0x3F);
    return cp;
  }
  if ((c & 0xF0) == 0xE0 && (str[0] & 0xC0) == 0x80 && (str[1] & 0xC0) == 0x80) {
    uint16_t cp = ((c & 0x0F) << 12) | ((str[0] & 0x3F) << 6) | (str[1] & 0x3F);
    str += 2;
    return cp;
  }
  return '?';
}

int SSD1306Display::findGlyph(uint16_t codepoint) {
  int lo = 0;
  int hi = utf8_10x10_font.count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    uint16_t val = pgm_read_word(&utf8_10x10_font.map[mid]);
    if (val == codepoint) return mid;
    if (val < codepoint) lo = mid + 1;
    else hi = mid - 1;
  }
  return -1;
}

void SSD1306Display::drawGlyph(int x, int y, uint16_t codepoint) {
  int index = findGlyph(codepoint);
  if (index < 0) {
    display.drawRect(x + 1, y + 1, 8, 8, _color);
    return;
  }

  const uint8_t w = utf8_10x10_font.w;
  const uint8_t h = utf8_10x10_font.h;
  const uint16_t bits_per_glyph = w * h;
  const uint16_t bytes_per_glyph = (bits_per_glyph + 7) / 8;
  const uint32_t data_offset = (uint32_t) index * bytes_per_glyph;
  for (uint8_t row = 0; row < h; row++) {
    for (uint8_t col = 0; col < w; col++) {
      uint16_t bit = row * w + col;
      uint8_t b = pgm_read_byte(&utf8_10x10_font.data[data_offset + bit / 8]);
      if (b & (1 << (bit % 8))) {
        display.drawPixel(x + col, y + row, _color);
      }
    }
  }
}

void SSD1306Display::print(const char* str) {
  while (*str) {
    const char* before = str;
    uint16_t cp = nextCodepoint(str);
    if (cp == '\n') {
      _cursorX = 0;
      _cursorY += 8 * _textSize;
      display.setCursor(_cursorX, _cursorY);
    } else if (cp < 0x80) {
      display.setCursor(_cursorX, _cursorY);
      display.write((uint8_t) cp);
      _cursorX += 6 * _textSize;
    } else {
      (void) before;
      drawGlyph(_cursorX, _cursorY, cp);
      _cursorX += utf8_10x10_font.w;
      display.setCursor(_cursorX, _cursorY);
    }
  }
}

void SSD1306Display::printWordWrap(const char* str, int max_width) {
  int line_start_x = _cursorX;
  while (*str) {
    const char* p = str;
    uint16_t cp = nextCodepoint(p);
    int char_w = (cp < 0x80) ? (6 * _textSize) : utf8_10x10_font.w;
    if (cp == '\n') {
      _cursorX = line_start_x;
      _cursorY += 10;
      display.setCursor(_cursorX, _cursorY);
      str = p;
      continue;
    }
    if (_cursorX > line_start_x && _cursorX + char_w > max_width) {
      _cursorX = line_start_x;
      _cursorY += 10;
      display.setCursor(_cursorX, _cursorY);
    }

    char tmp[5];
    uint8_t len = p - str;
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, str, len);
    tmp[len] = 0;
    print(tmp);
    str = p;
  }
}

void SSD1306Display::fillRect(int x, int y, int w, int h) {
  display.fillRect(x, y, w, h, _color);
}

void SSD1306Display::drawRect(int x, int y, int w, int h) {
  display.drawRect(x, y, w, h, _color);
}

void SSD1306Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display.drawBitmap(x, y, bits, w, h, _color);
}

void SSD1306Display::drawNativeBuffer(const uint8_t* buffer, size_t length) {
  if (!buffer) return;
  const size_t display_size = 128 * 64 / 8;
  memcpy(display.getBuffer(), buffer, length < display_size ? length : display_size);
}

uint16_t SSD1306Display::getTextWidth(const char* str) {
  uint16_t width = 0;
  while (*str) {
    uint16_t cp = nextCodepoint(str);
    if (cp == '\n') break;
    width += (cp < 0x80) ? (6 * _textSize) : utf8_10x10_font.w;
  }
  return width;
}

void SSD1306Display::endFrame() {
  display.display();
}
