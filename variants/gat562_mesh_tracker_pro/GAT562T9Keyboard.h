#pragma once

#include <Arduino.h>
#include <Wire.h>

class GAT562T9Keyboard {
public:
  bool begin(TwoWire* wire = &Wire);
  bool poll(char& character);
  void resetInputState();
  bool isAvailable() const { return _available; }

private:
  static const uint8_t ADDRESS = 0x34;
  static const uint8_t QUEUE_SIZE = 8;

  TwoWire* _wire = NULL;
  bool _available = false;
  bool _held = false;
  uint8_t _lastKey = 0xFF;
  uint8_t _heldKey = 0xFF;
  uint8_t _charIndex = 0;
  uint32_t _lastTap = 0;
  uint32_t _pressedAt = 0;
  uint32_t _lastPoll = 0;
  char _queue[QUEUE_SIZE];
  uint8_t _queueHead = 0;
  uint8_t _queueTail = 0;

  bool readRegister(uint8_t reg, uint8_t& value);
  bool writeRegister(uint8_t reg, uint8_t value);
  void configure();
  void processEvent(uint8_t event);
  void keyPressed(uint8_t key);
  void keyReleased();
  void enqueue(char character);
  bool dequeue(char& character);
};
