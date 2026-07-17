# Esc liveness reproduction

This fixture reproduces the overlay-close failure with the exact local file:

`/Users/ryougi/Downloads/markdown-formula.md`

The currently reviewed input is 26,798 bytes with SHA-256
`61adb310d77d5b497de6aaeee709c08d172510b5010ec7833842e37e7b8613fe`.
The hash is not hard-coded as an acceptance value: every build embeds the
current bytes from that path, and the verifier compares the calculator's
fixture marker with the current host file.

PocketJS's current single-program Firebird path transfers the launchable
`.tns` artifact, not an arbitrary companion document. The fixture therefore
stores the source bytes inside that artifact, writes them to the calculator's
document filesystem before `run_reader`, and verifies the written file before
opening it. This preserves the exact input without adding a second, ambiguous
TI-OS transfer flow.

Run:

```sh
make firebird-esc-liveness-test
```

Override either local dependency when necessary:

```sh
MARKDOWN_FORMULA_DOCUMENT=/absolute/path/to/markdown-formula.md \
POCKETJS_NSPIRE=/absolute/path/to/PocketJS-NSpire \
make firebird-esc-liveness-test
```

## Reproduction sequence

1. Generate a C++ byte array from the exact Markdown file without normalizing
   UTF-8, line endings, or trailing bytes.
2. Launch the dedicated Ndless integration build in Firebird.
3. Write those bytes to `/documents/markdown-formula.md.tns`, read the file
   back, and verify its byte count and FNV-1a checksum before opening it.
4. Wait for the application to report read, parse, first render, and present.
5. The exact Chinese document automatically opens its `CJK font needed` hint.
   Press Esc, wait for a complete release interval, then press Down; require
   the physical scan, semantic event, render, and presentation in that order.
6. Only after the hint is gone, press Doc (the production section-list key) to open the
   actual contents menu.
7. Press Esc, wait for another complete release interval, then press Down;
   require the second scan, event, render, and presentation sequence.
8. Press Ctrl+Esc and require a clean `EXIT_OK` return to TI-OS. PocketJS's
   Firebird frontend explicitly implements `ctrl-esc` as one scripted action
   that presses and releases both matrix keys.

The app is run with state persistence disabled so a storage warning cannot
hide or replace the overlay/input-loop failure under investigation.

## Stage diagnosis

The verifier always writes:

- `build/firebird-esc-liveness/serial.log`
- `build/firebird-esc-liveness/stage-diagnosis.txt`
- `build/firebird-esc-liveness/frame.ppm`

Paired markers distinguish the blocking boundary:

- `TRACE_POLL_ENTER` without `TRACE_POLL_RETURN`: input polling blocked.
- `TRACE_SLEEP_ENTER` without `TRACE_SLEEP_RETURN`: idle sleep blocked.
- `RENDER_START` without `RENDER_DONE`: viewer rendering blocked.
- `TRACE_PRESENT_ENTER` without `TRACE_PRESENT_RETURN`: LCD presentation
  blocked.
- `TRACE_LOWLEVEL_POST_HINT_DOWN_SCAN` without
  `TRACE_POST_HINT_DOWN_EVENT`: hint recovery reached hardware input but did
  not return a semantic Down event.
- `TRACE_LOWLEVEL_POST_TOC_DOWN_SCAN` without
  `TRACE_POST_TOC_DOWN_EVENT`: TOC recovery reached hardware input but did not
  return a semantic Down event.
- `TRACE_POST_TOC_DOWN_EVENT` without `TRACE_CTRL_ESC_QUIT_EVENT`: both overlay
  recoveries completed, but the later exit chord did not reach the reader.
- `TRACE_CTRL_ESC_QUIT_EVENT` without `EXIT_OK`: input recovered, but shutdown
  did not return. `TRACE_DISPLAY_SHUTDOWN_ENTER/RETURN` further separates
  viewer teardown from LCD shutdown.

Firebird remains a simulator check. The same `.tns` artifact can be copied to
real hardware for comparison, but cache, keypad, and touchpad conclusions must
still be confirmed on a CX/CX II calculator.
