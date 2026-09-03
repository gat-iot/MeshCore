#include "GAT562T9Keyboard.h"

#ifdef GAT562_T9_KEYBOARD

namespace {
constexpr uint8_t REG_CFG = 0x01;
constexpr uint8_t REG_INT_STAT = 0x02;
constexpr uint8_t REG_KEY_LCK_EC = 0x03;
constexpr uint8_t REG_KEY_EVENT_A = 0x04;
constexpr uint8_t REG_GPIO_DIR_1 = 0x23;
constexpr uint8_t REG_GPI_EM_1 = 0x20;
constexpr uint8_t REG_GPIO_INT_EN_1 = 0x1A;
constexpr uint8_t REG_GPIO_INT_LVL_1 = 0x26;
constexpr uint8_t REG_KP_GPIO_1 = 0x1D;
constexpr uint8_t REG_KP_GPIO_2 = 0x1E;
constexpr uint8_t REG_KP_GPIO_3 = 0x1F;
constexpr uint8_t REG_DEBOUNCE_DIS_1 = 0x29;

constexpr uint8_t ROWS = 5;
constexpr uint8_t COLS = 3;
constexpr uint8_t KEY_COUNT = 12;
constexpr uint32_t MULTI_TAP_MILLIS = 750;
constexpr uint32_t LONG_PRESS_MILLIS = 2000;

const uint8_t TAP_COUNTS[KEY_COUNT] = {7, 7, 7, 7, 7, 7, 9, 7, 9, 2, 2, 2};
const char TAP_MAP[KEY_COUNT][9] = {
  {',', '.', '!', '?', '<', '>', '1'},
  {'a', 'b', 'c', 'A', 'B', 'C', '2'},
  {'d', 'e', 'f', 'D', 'E', 'F', '3'},
  {'g', 'h', 'i', 'G', 'H', 'I', '4'},
  {'j', 'k', 'l', 'J', 'K', 'L', '5'},
  {'m', 'n', 'o', 'M', 'N', 'O', '6'},
  {'p', 'q', 'r', 's', 'P', 'Q', 'R', 'S', '7'},
  {'t', 'u', 'v', 'T', 'U', 'V', '8'},
  {'w', 'x', 'y', 'z', 'W', 'X', 'Y', 'Z', '9'},
  {'*', '+'},
  {' ', '0'},
  {'#', '^'},
};
}

bool GAT562T9Keyboard::begin(TwoWire* wire) {
  _wire = wire;

  pinMode(GAT562_T9_RST_PIN, OUTPUT);
  digitalWrite(GAT562_T9_RST_PIN, LOW);
  delay(5);
  digitalWrite(GAT562_T9_RST_PIN, HIGH);
  delay(20);
  pinMode(GAT562_T9_INT_PIN, INPUT_PULLUP);

  uint8_t value = 0;
  _available = readRegister(REG_KEY_LCK_EC, value);
  if (_available) configure();
  return _available;
}

bool GAT562T9Keyboard::poll(char& character) {
  if (dequeue(character)) return true;
  if (!_available) return false;

  const uint32_t now = millis();
  if (digitalRead(GAT562_T9_INT_PIN) != LOW && now - _lastPoll < 50) return false;
  _lastPoll = now;

  uint8_t count = 0;
  if (!readRegister(REG_KEY_LCK_EC, count)) {
    _available = false;
    return false;
  }

  count &= 0x0F;
  while (count-- > 0) {
    uint8_t event = 0;
    if (!readRegister(REG_KEY_EVENT_A, event)) {
      _available = false;
      return false;
    }
    processEvent(event);
  }
  writeRegister(REG_INT_STAT, 0x03);
  return dequeue(character);
}

void GAT562T9Keyboard::resetInputState() {
  _held = false;
  _lastKey = 0xFF;
  _heldKey = 0xFF;
  _charIndex = 0;
  _lastTap = 0;
  _pressedAt = 0;
  _queueHead = 0;
  _queueTail = 0;
}

bool GAT562T9Keyboard::readRegister(uint8_t reg, uint8_t& value) {
  if (!_wire) return false;
  _wire->beginTransmission(ADDRESS);
  _wire->write(reg);
  if (_wire->endTransmission() != 0) return false;
  if (_wire->requestFrom(ADDRESS, (uint8_t) 1) != 1) return false;
  value = _wire->read();
  return true;
}

bool GAT562T9Keyboard::writeRegister(uint8_t reg, uint8_t value) {
  if (!_wire) return false;
  _wire->beginTransmission(ADDRESS);
  _wire->write(reg);
  _wire->write(value);
  return _wire->endTransmission() == 0;
}

void GAT562T9Keyboard::configure() {
  for (uint8_t i = 0; i < 3; i++) {
    writeRegister(REG_GPIO_DIR_1 + i, 0x00);
    writeRegister(REG_GPI_EM_1 + i, 0xFF);
    writeRegister(REG_GPIO_INT_LVL_1 + i, 0x00);
    writeRegister(REG_GPIO_INT_EN_1 + i, 0xFF);
    writeRegister(REG_DEBOUNCE_DIS_1 + i, 0x00);
  }

  writeRegister(REG_KP_GPIO_1, 0x1F);
  writeRegister(REG_KP_GPIO_2, 0x07);
  writeRegister(REG_KP_GPIO_3, 0x00);

  uint8_t count = 0;
  if (readRegister(REG_KEY_LCK_EC, count)) {
    count &= 0x0F;
    while (count-- > 0) {
      uint8_t ignored = 0;
      readRegister(REG_KEY_EVENT_A, ignored);
    }
  }
  writeRegister(REG_INT_STAT, 0x03);

  uint8_t cfg = 0;
  if (readRegister(REG_CFG, cfg)) writeRegister(REG_CFG, cfg | 0x01);
}

void GAT562T9Keyboard::processEvent(uint8_t event) {
  const uint8_t key = event & 0x7F;
  if (event & 0x80) keyPressed(key);
  else keyReleased();
}

void GAT562T9Keyboard::keyPressed(uint8_t key) {
  const int row = (key - 1) / 10;
  const int col = (key - 1) % 10;
  if (key == 0 || col >= COLS || row < 0 || row >= ROWS || row == 3) return;

  const uint8_t nextKey = (uint8_t) ((row < 3 ? row : 3) * COLS + col);
  const uint32_t now = millis();
  if (nextKey != _lastKey || now - _lastTap > MULTI_TAP_MILLIS) _charIndex = 0;
  else _charIndex++;

  _lastKey = nextKey;
  _heldKey = nextKey;
  _lastTap = now;
  _pressedAt = now;
  _held = true;
}

void GAT562T9Keyboard::keyReleased() {
  if (!_held || _heldKey >= KEY_COUNT) return;

  const uint32_t now = millis();
  const uint8_t index = (now - _pressedAt > LONG_PRESS_MILLIS)
    ? 0
    : (_charIndex % TAP_COUNTS[_heldKey]);
  enqueue(TAP_MAP[_heldKey][index]);
  _lastTap = now;
  _held = false;
  _heldKey = 0xFF;
}

void GAT562T9Keyboard::enqueue(char character) {
  const uint8_t next = (uint8_t) ((_queueTail + 1) % QUEUE_SIZE);
  if (next == _queueHead) return;
  _queue[_queueTail] = character;
  _queueTail = next;
}

bool GAT562T9Keyboard::dequeue(char& character) {
  if (_queueHead == _queueTail) return false;
  character = _queue[_queueHead];
  _queueHead = (uint8_t) ((_queueHead + 1) % QUEUE_SIZE);
  return true;
}

#else

bool GAT562T9Keyboard::begin(TwoWire*) { return false; }
bool GAT562T9Keyboard::poll(char&) { return false; }
void GAT562T9Keyboard::resetInputState() {}

#endif
