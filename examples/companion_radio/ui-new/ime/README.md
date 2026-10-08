# Input Method Sources

The Bopomofo engine, dictionary helpers, prediction helper and dictionary data were imported from the local Meshtastic `firmware-merge-pr3` source at commit `0ab158d`, matching the `gat562-t9-candidates-prediction-test` builds supplied for this port. The source Meshtastic project is GPL-3.0; its license is included as `LICENSE-Meshtastic.txt`. Original attribution in the imported files is retained. Firmware binaries incorporating this GPL-derived input method are distributed under GPL-3.0 with corresponding source in this repository; the existing MIT notices for MeshCore remain unchanged.

`gat562_pinyin_predictions.h` is the exact 5,008-word, 40 KB prediction pool from the supplied build, derived from the sources credited in its header. `zhuyin_single_static.h` is the flash-limited McBopomofo-based dictionary used by the supplied TW build. It includes a bounded phrase set as well as single characters. This port does not use an online dictionary or claim exhaustive word coverage.

The new MeshCore editor adapts the source behavior without importing Meshtastic's mesh stack, device drivers, USB initialization or board power management.
