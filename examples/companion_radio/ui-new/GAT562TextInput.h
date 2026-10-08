#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#ifdef GAT562_CH_UI
#include <pinyin_simple_backend.h>
#include "ime/PinyinPrediction.h"
#elif defined(GAT562_TW_UI)
#include "ime/bpmf_engine.h"
#endif

class GAT562TextInput {
  std::string _text, _composition;
  std::vector<std::string> _candidates;
  std::vector<size_t> _pages;
  size_t _selected = 0, _page = 0, _limit = 120;
  bool _ime = false, _prediction = false, _inserted = false;
  uint8_t _lastGroup = 0;
  uint32_t _lastTap = 0;
#ifdef GAT562_TW_UI
  bpmf::Engine _bpmf;
#endif

  static void eraseLast(std::string& s) {
    if (s.empty()) return;
    size_t i = s.size() - 1;
    while (i && (uint8_t(s[i]) & 0xc0) == 0x80) --i;
    s.erase(i);
  }
  static std::string lastChar(const std::string& s) {
    std::string prefix = s;
    eraseLast(prefix);
    return s.substr(prefix.size());
  }
  void refresh() {
    _candidates.clear();
    _selected = _page = 0;
    _pages.clear();
    _prediction = _ime && _composition.empty();
    if (_ime) {
#ifdef GAT562_CH_UI
      if (_prediction) _candidates = gat562PredictAfter(lastChar(_text));
      else {
        const char* found = pinyin_simple_search(_composition.c_str());
        if (found) {
          std::string chars(found);
          for (size_t i = 0; i < chars.size() && _candidates.size() < 64;) {
            const size_t n = charSize(uint8_t(chars[i]));
            if (i + n > chars.size()) break;
            _candidates.push_back(chars.substr(i, n));
            i += n;
          }
        }
      }
#elif defined(GAT562_TW_UI)
      if (_prediction) _bpmf.predictAfter(lastChar(_text));
      else _bpmf.searchFor(_composition);
      _candidates = _bpmf.candidates();
#endif
    }
    if (_prediction) {
      const auto prefix = lastChar(_text);
      for (auto& word : _candidates)
        if (!prefix.empty() && word.compare(0, prefix.size(), prefix) == 0) word.erase(0, prefix.size());
    }
    // Page boundaries use the rendered suffix widths, shared by all four directions.
    unsigned used = 0, count = 0;
    for (size_t i = 0; i < _candidates.size(); ++i) {
      unsigned w = width(_candidates[i]) + 3;
      if (!count || count == 9 || used + w > 118) {
        _pages.push_back(i); used = count = 0;
      }
      used += w; ++count;
    }
  }

public:
  static size_t charSize(uint8_t c) { return c < 128 ? 1 : (c < 224 ? 2 : (c < 240 ? 3 : 4)); }
  static unsigned width(const std::string& s) {
    unsigned w = 0;
    for (size_t i = 0; i < s.size();) {
      const size_t n = charSize(uint8_t(s[i]));
      w += n == 1 ? 6 : 10; i += n;
    }
    return w;
  }
  static uint8_t group(char c) {
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (c >= 'a' && c <= 'o') return 2 + (c - 'a') / 3;
    if (c >= 'p' && c <= 's') return 7;
    if (c >= 't' && c <= 'v') return 8;
    if (c >= 'w' && c <= 'z') return 9;
    if (c >= '2' && c <= '9') return c - '0';
    if (c == '1' || c == '<' || c == '>' || c == '.' || c == ',' || c == '?' || c == '!') return 1;
    if (c == '*' || c == '+') return 10;
    if (c == ' ' || c == '0') return 11;
    if (c == '#' || c == '^') return 12;
    return 0;
  }
  void endCycle() { _lastGroup = 0; _inserted = false; }
  void reset(const char* initial = "", size_t limit = 120) {
    _limit = limit;
    _text = initial ? initial : "";
    while (_text.size() > _limit) eraseLast(_text);
    _composition.clear();
#if defined(GAT562_TW_UI)
    _ime = true;
#else
    _ime = false;
#endif
    endCycle(); refresh();
  }
  void toggle() {
#if defined(GAT562_CH_UI) || defined(GAT562_TW_UI)
    _ime = !_ime;
    _composition.clear(); endCycle(); refresh();
#endif
  }
  const char* language() const {
    if (!_ime) return "EN";
#ifdef GAT562_TW_UI
    return "TW";
#else
    return "CN";
#endif
  }
  const std::string& text() const { return _text; }
  const std::string& composition() const { return _composition; }
  const std::vector<std::string>& candidates() const { return _candidates; }
  size_t selected() const { return _selected; }
  size_t pageStart() const { return _pages.empty() ? 0 : _pages[_page]; }
  size_t pageEnd() const { return _page + 1 < _pages.size() ? _pages[_page+1] : _candidates.size(); }
  bool multiplePages() const { return _pages.size() > 1; }
  void move(int direction) {
    endCycle();
    if (_candidates.empty()) return;
    if (direction < 0) _selected = _selected ? _selected-1 : _candidates.size()-1;
    else _selected = (_selected+1) % _candidates.size();
    _page = 0;
    while (_page+1 < _pages.size() && _pages[_page+1] <= _selected) ++_page;
  }
  bool select() {
    if (_candidates.empty()) { endCycle(); return false; }
    const auto word = _candidates[_selected];
    if (_text.size() + word.size() > _limit) { endCycle(); return false; }
    _text += word;
    _composition.clear(); endCycle(); refresh();
    return true;
  }
  void backspace() {
    if (!_composition.empty()) eraseLast(_composition);
    else eraseLast(_text);
    endCycle(); refresh();
  }
  bool input(char c, uint32_t now) {
    const uint8_t g = group(c);
    std::string text = _text, composing = _composition;
    if (g && g == _lastGroup && _inserted && uint32_t(now - _lastTap) < 850) {
      if (_ime && !composing.empty()) eraseLast(composing);
      else eraseLast(text);
    }
    bool inserted = false;
    if (_ime && c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (_ime) {
#ifdef GAT562_TW_UI
      // Keep the source firmware's physical-key to on-screen Bopomofo mapping.
      if (c == ' ' && !composing.empty()) {
        composing += bpmf::tone5_symbol()->utf8; inserted = true;
      } else {
        // Unlike pinyin, digits are actual Bopomofo keys. T9 has no semicolon key.
        const char mapped = c == '#' || c == '^' ? ';' : c;
        if (const auto sym = bpmf::screen_symbol(mapped)) { composing += sym->utf8; inserted = true; }
        else if (composing.empty()) { text += c; inserted = true; }
      }
#elif defined(GAT562_CH_UI)
      if (c >= 'a' && c <= 'z') { composing += c; inserted = true; }
      else if (!(c >= '2' && c <= '9') && composing.empty()) { text += c; inserted = true; }
#endif
    } else { text += c; inserted = true; }
    if (text.size() > _limit || composing.size() > 48 || text.size() + composing.size() > _limit) {
      endCycle(); return false;
    }
    _text.swap(text); _composition.swap(composing);
    _lastGroup = g; _lastTap = now; _inserted = inserted;
    refresh(); return true;
  }
};
