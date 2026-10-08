#pragma once
#include "UITask.h"
#include "GAT562TextInput.h"

class GAT562T9Editor : public UIScreen {
  UITask* _task;
  GAT562TextInput _input;
  bool _preset = false;
  uint8_t _presetIndex = 0;

  void endCycle() {
    _input.endCycle();
    t9_keyboard.resetInputState();
  }
  static std::vector<std::string> lines(const std::string& text, unsigned width) {
    std::vector<std::string> result(1);
    unsigned used = 0;
    for (size_t i = 0; i < text.size();) {
      size_t n = GAT562TextInput::charSize(uint8_t(text[i]));
      if (i+n > text.size()) break;
      unsigned w = n == 1 ? 6 : 10;
      if (used+w > width) { result.emplace_back(); used = 0; }
      result.back().append(text, i, n); used += w; i += n;
    }
    return result;
  }

public:
  explicit GAT562T9Editor(UITask* task) : _task(task) { reset(); }
  void reset(const char* initial = "", bool preset = false, uint8_t index = 0) {
    _preset = preset; _presetIndex = index;
    _input.reset(initial, preset ? UI_PRESET_MSG_BYTES : 120);
    endCycle();
  }
  int render(DisplayDriver& d) override {
    d.setTextSize(1); d.setColor(UIColor::secondary_txt);
    d.setCursor(0, 0); d.print(_preset ? "Preset" : "Message");
    d.drawTextRightAlign(d.width()-1, 0, _input.language());
    const auto textLines = lines(_input.text(), d.width()-2);
    size_t first = textLines.size() > 2 ? textLines.size()-2 : 0;
    d.setColor(UIColor::primary_txt);
    for (size_t i = first; i < textLines.size(); ++i) {
      d.setCursor(0, 13 + int(i-first)*12); d.print(textLines[i].c_str());
    }
    const auto compLines = lines(_input.composition(), d.width()-2);
    d.setColor(UIColor::warning_txt);
    d.setCursor(0, 38); d.print(compLines.back().c_str());
    d.setColor(UIColor::primary_txt);
    int x = 0;
    for (size_t i = _input.pageStart(); i < _input.pageEnd(); ++i) {
      const auto& word = _input.candidates()[i];
      int w = d.getTextWidth(word.c_str());
      d.drawTextEllipsized(x, 51, 118-x, word.c_str());
      if (i == _input.selected()) d.fillRect(x, 62, w < 118-x ? w : 118-x, 2);
      x += w + 3;
    }
    if (_input.multiplePages()) { d.setCursor(122, 51); d.print(">"); }
    return 250;
  }
  bool handleInput(int c) override {
    if (c == KEY_T9_BACKSPACE) { _input.backspace(); endCycle(); }
    else if (c == KEY_T9_EXIT) { endCycle(); _task->gotoHomeScreen(); }
    else if (c >= 0x20 && c <= 0x7e) {
      if (!_input.input(char(c), millis())) _task->showAlert("Message full", 900);
    } else if (c == KEY_UP || c == KEY_LEFT || c == KEY_PREV) { _input.move(-1); endCycle(); }
    else if (c == KEY_DOWN || c == KEY_RIGHT || c == KEY_NEXT) { _input.move(1); endCycle(); }
    else if (c == KEY_ENTER || c == KEY_SELECT) {
      if (!_input.candidates().empty() && !_input.select()) _task->showAlert("Message full", 900);
      endCycle();
    } else if (c == KEY_CANCEL) { _input.toggle(); endCycle(); }
    else if (c == KEY_LONG_ENTER || c == KEY_CONTEXT_MENU) {
      endCycle();
      if (!_input.composition().empty()) _task->showAlert("Select a candidate", 1000);
      else if (_preset) _task->savePresetMessage(_presetIndex, _input.text().c_str());
      else if (_input.text().empty()) _task->showAlert("Empty message", 900);
      else _task->sendComposedPublicText(_input.text().c_str());
    } else return false;
    return true;
  }
};
