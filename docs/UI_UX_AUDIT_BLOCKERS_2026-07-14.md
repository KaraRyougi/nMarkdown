# UI/UX Audit Hardware Blockers — 2026-07-14

These are the audit steps that could not be proven with a real TI-Nspire CX or
CX II in this run. PocketJS/Firebird executes the calculator binary and was used
where its key tape supports the interaction. The desktop harness was used for
semantic behavior. Neither backend can substitute for the physical checks
listed below, so the dated audit does not claim them as passing.

## BLOCKED-SET-physical-ctrl-t-plus-minus

PocketJS cannot send `Ctrl+T`, Plus, or Minus. The Settings sequences and font
size changes passed semantically, and the Dark-mode sequence passed in Firebird
through a fixture-only Settings alias, but physical chord recognition and the
Plus/Minus keys still need a calculator run.

Close this blocker on hardware by opening Settings with `Ctrl+T`, changing and
closing every setting, scrolling, reopening it, and exercising Plus/Minus at
12 px and 22 px. Capture the calculator screen after every state change.

## BLOCKED-NAV-02-physical-ctrl-esc

PocketJS exposes a standalone Ctrl key but cannot hold Ctrl while pressing Esc.
The semantic `Quit` event and the production input-priority source path were
verified, but an emulator frame cannot prove the physical chord.

Close this blocker by pressing `Ctrl+Esc` from the document, wide focus, every
overlay, both browser contexts, and every dialog on hardware. Confirm return to
TI-OS and a readable `.nmdstate` after each run.

## BLOCKED-SRCH-01-physical-entry

PocketJS cannot inject letters, Shift+letters, physical 0/1, Space, Period, or
Delete. The current native keymap fixture verifies that physical 2/4/6/7/8 pass
through production `InputNdless` as query text in Search and resume reader
navigation immediately after Esc. Search modes, query retention, navigation,
and the 64-byte semantic limit are also covered, but the complete physical
keyboard-entry matrix remains unproven.

Close this blocker by entering lowercase and uppercase text, digits, a space,
a period, and Delete in Search on hardware. Also try punctuation and CJK input
to confirm that the documented limitations are visible and non-destructive.

## BLOCKED-INPUT-01-physical-overlay-matrix

PocketJS does not expose Menu, Plus/Minus, bookmark/search chords, or enough
overlay-opening chords to reproduce the full isolation matrix. The semantic
matrix found real background-input leaks, but physical key and touch priority
must still be checked on the device.

Close this blocker by opening each overlay in Scroll and Page Swipe modes, then
trying arrows, modified arrows, touch drag, both swipe directions, Plus/Minus,
Enter, `Ctrl+B`, and `Ctrl+N`. Capture the open overlay and the screen after it
closes so hidden document movement can be compared.

## BLOCKED-INPUT-02-timing-and-chords

The desktop harness emits discrete semantic events, and PocketJS presses and
releases one ordinary key at a time. Neither backend can measure the 360 ms
initial repeat delay, 70 ms repeat interval, overlapping modifiers, or
keyboard-versus-touch arbitration.

Close this blocker with a video or timestamped device trace of held arrows,
held Delete, held characters, simultaneous Ctrl/Esc and Shift/arrows, and a
touch gesture started while a key is held.

## BLOCKED-STATE-03-calculator-permissions

Malformed, stale, and checksummed sidecars were exercised on desktop. Desktop
permissions and atomic replacement are not a reliable model of the calculator
filesystem, so unreadable/unwritable sidecars need device confirmation.

Close this blocker by testing a read-only location or controlled write failure
on expendable hardware data, then confirming that the document remains usable,
the app exits, and no unrelated file is altered.

## BLOCKED-ERR-02-font-fixtures

Invalid, truncated, and oversized font files were exercised for Body,
Monospace, and CJK. Two planned failure classes could not be made deterministic
with the current one-shot harness: a valid font whose outline technology is
unsupported by this FreeType build, and a file removed after the menu lists it
but before Enter reads it.

Close this blocker with a known unsupported-outline fixture and an interactive
test hook that removes a disposable font between list and selection. Confirm
that all three roles retain their prior font and that the dialog names the real
failure.
