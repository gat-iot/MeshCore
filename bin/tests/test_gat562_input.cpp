#include "../../examples/companion_radio/ui-new/GAT562TextInput.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#ifdef GAT562_CH_UI
extern "C" char* pinyin_simple_search(const char* text) {
  static char ni[] = u8"\u4f60\u6ce5\u59ae\u62df\u9006\u502a\u533f\u817b\u6eba\u5c3c\u5462\u6635\u9713\u576d\u65ce";
  static char zhong[] = u8"\u4e2d\u79cd\u91cd";
  if (!strcmp(text, "ni")) return ni;
  if (!strcmp(text, "zhong")) return zhong;
  return nullptr;
}
#endif
static void pages(GAT562TextInput& input) {
  assert(!input.candidates().empty());
  size_t total = input.candidates().size();
  for (size_t i = 0; i < total; ++i) {
    const size_t selected = input.selected(), start = input.pageStart(), end = input.pageEnd();
    assert(start <= selected && selected < end && end <= total);
    unsigned width = 0;
    for (size_t j = start; j < end; ++j) width += GAT562TextInput::width(input.candidates()[j]) + 3;
    assert(width <= 118 || end == start+1);
    input.move(1); input.move(-1);
    assert(input.selected() == selected && input.pageStart() == start && input.pageEnd() == end);
    input.move(1);
  }
  assert(input.selected() == 0);
}
static void chinese(GAT562TextInput& input) {
  if (!strcmp(input.language(), "EN")) input.toggle();
}
static void english(GAT562TextInput& input) {
  if (strcmp(input.language(), "EN")) input.toggle();
}
int main() {
  GAT562TextInput input;
  input.reset();
  english(input);
  assert(!strcmp(input.language(), "EN"));
  assert(input.input('a', 1000));
  assert(input.input('b', 1100));
  assert(input.text() == "b");
  assert(input.input('A', 1200));
  assert(input.text() == "A");
  input.endCycle(); assert(input.input('a', 1300));
  assert(input.text() == "Aa");
  input.reset("123", 3);
  english(input);
  assert(!input.input('d', 2000)); assert(input.text() == "123");
  input.backspace(); assert(input.text() == "12");
  input.reset();
#if defined(GAT562_CH_UI)
  chinese(input);
  assert(input.input('n', 3000)); assert(input.input('i', 4000));
  assert(input.composition() == "ni");
  pages(input); assert(input.select());
  assert(input.text() == u8"\u4f60");
  assert(input.input('i', 4100));
  assert(input.text() == u8"\u4f60" && input.composition() == "i");
  input.backspace(); input.backspace(); assert(input.text().empty());
#elif defined(GAT562_TW_UI)
  assert(input.input('s', 3000)); assert(input.input('u', 4000));
  assert(input.composition() == u8"\u310b\u3127");
  pages(input); assert(input.select());
  const auto chosen = input.text();
  assert(!chosen.empty());
  assert(input.input('u', 4100)); assert(input.text() == chosen);
  assert(input.composition() == u8"\u3127");
  input.backspace(); assert(input.composition().empty());
  input.reset();
  input.input('2', 5000); assert(input.composition() == u8"\u3109");
  input.input('#', 6000); assert(input.composition() == u8"\u3109\u3124");
  input.backspace(); assert(input.composition() == u8"\u3109");
#endif
#if defined(GAT562_CH_UI) || defined(GAT562_TW_UI)
  input.reset(u8"\u4e2d");
  chinese(input);
  pages(input);
  const auto suffix = input.candidates()[0];
  assert(input.select());
  assert(input.text() == std::string(u8"\u4e2d") + suffix);
  input.reset(u8"\u4e2d", 3);
  chinese(input);
  assert(!input.select()); assert(input.text() == u8"\u4e2d");
  input.backspace(); assert(input.text().empty());
  input.reset("ab", 3);
  chinese(input);
  input.input('s', 8000); input.input('u', 9000);
  assert(input.text().size()+input.composition().size() <= 3);
#endif
  input.reset();
  english(input);
  input.input('a', 0xffffff00UL); input.input('b', 0x50UL);
  assert(input.text() == "b");
  puts("GAT562 input tests passed");
}
