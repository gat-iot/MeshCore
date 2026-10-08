# GAT562 T9 Input

The T9 EN, CH and new TW builds use a physical-keyboard-only editor. Family builds retain their existing on-screen keyboard. No radio, USB, BLE, alarm or message-routing settings are changed by the input port.

- Pressing a T9 key outside the editor opens it and consumes that first key, as before.
- CH uses pinyin with a bounded common-word prediction table; USER switches CN/EN. It starts in EN, matching the existing CH editor.
- EN retains lower/uppercase/digit multi-tap cycling and does not include a Chinese font or dictionary.
- TW uses the supplied Bopomofo engine and traditional font; USER switches TW/EN. It starts in TW. Existing non-editor menus remain English.
- TW digit positions in the multi-tap cycle enter the mapped Bopomofo symbol/tone instead of being suppressed as pinyin digits. `#` maps to the semicolon symbol (U+3124) absent from the physical keypad. Space enters the neutral tone while composing.
- Up/left selects the previous candidate; down/right selects the next. The underline covers the selected word. Paging uses the actual displayed widths in both directions.
- Short confirm commits a candidate. Predictions display and append only the continuation, without duplicating the preceding character. Selecting or navigating ends the previous multi-tap cycle.
- Long confirm sends the committed text, or saves a preset. An unfinished composition must be selected or deleted before sending.
- DEL deletes one composition symbol or committed UTF-8 character. Long DEL exits without sending.
- Message size remains 120 UTF-8 bytes; preset editing is limited to 40 bytes. A candidate that does not fit is not partially inserted.

The new TW environment inherits the current CH radio defaults (480.375 MHz, SF10, BW62.5, CR5, 17 dBm), but does not change any settings already saved on the device. EN retains its existing defaults. Source dictionaries and font notices are retained.

Host tests exercise the actual editor state model, real prediction and Bopomofo dictionaries, mixed-length candidate paging, UTF-8 bounds and multi-tap reset. Pinyin character lookup is stubbed in host tests; the firmware links the existing pinyin library. All five build variants must pass USB and keyboard/language isolation checks. Real-device input, display, USB and BLE tests remain required.
