# nMarkdown UI/UX Audit Resolution — 2026-07-14

> **Superseded evidence snapshot:** this report contains package hashes,
> Firebird results, and a 16-test count from an earlier post-audit build. Use
> the [current completion record](UI_UX_AUDIT_COMPLETION_2026-07-14.md) for the
> present 21-test suite, current font baseline, explicit native/Firebird
> verification fields, and hardware limitations.

## Status

All 16 P1/P2 findings recorded in the dated
[UI/UX audit](UI_UX_AUDIT_2026-07-14.md) have an implemented resolution and
host regression coverage in the current tree. The original audit is preserved
unchanged as a record of the build that was reviewed: its **Current** sections,
screenshots, counts, and release-hold verdict describe that earlier build, not
the implementation documented here.

No P1/P2 item remains open in the host implementation. The remaining
verification work is limited to behavior that a desktop semantic harness or
Firebird cannot prove on physical CX/CX II hardware; see
[Physical-hardware verification still required](#physical-hardware-verification-still-required).

The complete current interaction contract and recovery instructions are in the
[user guide](USER_GUIDE.md).

## Current rendering and font baseline

The resolved build uses the following role stack:

- **Body:** embedded DejaVu Sans.
- **Monospace:** embedded DejaVu Sans Mono.
- **CJK:** no embedded default; the optional calculator-sized asset is Sarasa
  Fixed SC Regular and is selected from the CJK role menu.
- **Math:** embedded Latin Modern Math, including OpenType MATH constants and
  variants.

Body, Monospace, and CJK can load transferred TrueType/OpenType files during an
app session. Latin Modern Math is fixed for formula symbols and Latin
annotations and does not appear in the
role picker. See [Optional fonts](USER_GUIDE.md#11-optional-fonts) for the exact
device workflow and size limits.

## Finding-by-finding resolution

| # | Original finding | Resolution in the current implementation | Host regression evidence |
|---:|---|---|---|
| 1 | **P1 — Inline math overlaps ordinary prose** | Inline formulas now contribute their measured ascent, descent, baseline, and horizontal advance to the containing line. Automatic line spacing grows for tall formula ink, and wrapping accounts for the formula advance before committing the run. Formula glyphs come from Latin Modern Math. | `phase2-block-layout`: `test_inline_and_display_math_integration`, `test_automatic_content_aware_line_spacing`, and the format-gallery cases. `phase3-math-layout` exercises the formula corpus; `phase3-math-golden` checks deterministic raster output. |
| 2 | **P1 — Visible overlays leak input to the document** | Input is dispatched to the topmost visible layer before wide focus or document commands. Page movement, pointer movement, font-size changes, bookmark changes, next-result actions, activation, and theme toggling are consumed while a modal overlay owns the screen. `Ctrl+Esc` remains the immediate global exit. | `phase0-core`: `test_modal_input_isolation_and_escape_order` checks Settings and Reader Controls against page, pointer, size, bookmark, activation, and hidden-theme changes. |
| 3 | **P1 — Empty Bookmarks activates an invisible TOC row** | TOC and Bookmark selection are isolated. Removing the last bookmark leaves an explicit empty state, clears/clamps activation state, and makes Enter a consumed no-op. | `phase0-core`: `test_toc_bookmark_empty_state_is_safe`. |
| 4 | **P1 — Failed direct launch silently shows the demonstration document** | A direct-open failure now stays in a dedicated error state that names the requested file, retains the original reason, and presents `Ctrl+O`/`Ctrl+Esc` recovery. It no longer substitutes demonstration content. | `phase0-core`: `test_binary_direct_open_has_visible_error_state`, direct wrapped-Markdown coverage, and application-lifecycle error paths. Missing/oversized behavior also remains covered by the desktop harness scenarios recorded in the audit plan. |
| 5 | **P1 — Restored Page Swipe state can open on a blank final page** | Page Swipe restoration resolves the saved source-relative anchor to the final content-bearing page and clamps semantic endpoints after lazy layout. Reaching the end still reports the real final page. | `phase0-core`: `test_page_swipe_restore_has_visible_content`, `test_scroll_and_page_swipe_modes`, and `test_page_swipe_lazy_growth_does_not_skip_a_page`. The restore test asserts document ink below the header on the last page. |
| 6 | **P2 — Esc clears hidden wide focus before visible Settings** | Back-stack order is now: immediate global exit, visible overlay/nested picker, wide-block focus, then document/application exit. One Esc closes Settings while preserving the covered wide focus. | `phase0-core`: the final sequence in `test_modal_input_isolation_and_escape_order` closes Settings first, then proves the underlying code block is still pannable before a later Esc clears focus. |
| 7 | **P2 — Generic Reader Controls behaves like a hidden command surface** | Reader Controls is a passive modal. Menu toggles it closed; Enter and page/pointer/document commands are consumed without changing theme or reading position. | `phase0-core`: `test_modal_input_isolation_and_escape_order` checks no hidden page or theme mutation and checks Menu close. |
| 8 | **P2 — Contrast palette changes while its label stays stale** | The settings rows are rebuilt when Contrast changes, so palette and visible value update in the same frame in both directions. | `phase0-core` exercises live Settings repaint and theme/contrast rendering paths. There is not a separate OCR assertion for the word in the row; the same-frame row rebuild is implementation-checked and included in the passing Settings suite. |
| 9 | **P2 — Oversized font reports a false missing-file error** | `.tns` fallback is attempted only for a genuinely missing path. A size-limit or font-decoding failure keeps the first, more specific reason, preserves the previous role face, and uses font-specific dialog wording. | `phase0-core`: `test_font_size_error_is_not_overwritten_by_retry` asserts one read, no `.tns` retry, and preservation of the size-limit path. Font role/load rollback is also exercised by `test_font_pack_menu`. |
| 10 | **P2 — Search exposes raw Markdown and clips useful context** | Result labels strip common heading/link/table punctuation, add containing-heading context, cut on UTF-8/word-safe boundaries, and use ellipses when context is omitted. The documented query limit is expressed as an approximately 64-byte calculator-input limit. | `phase4-search`: `test_utf8_safe_snippet_and_limit` and `test_snippets_are_reader_facing`, plus exact/ASCII/canonical/Unicode-fold matching tests. |
| 11 | **P2 — Document browser is ambiguous and silently capped** | Duplicate document basenames now append the shortest distinguishing parent suffix. Reaching the 256-document cap adds a visible, non-selectable warning row. Recursive flat browsing remains an intentional compact-screen scope choice rather than a silent ambiguity. | `phase0-core`: `test_document_browser` renders duplicate-parent labels, pages to the cap warning, and proves activation still selects the final real file rather than the warning row. |
| 12 | **P2 — Wrapped code falsely accepts wide focus** | Wide focus is based on measured post-layout horizontal overflow, not merely on block type. Wrapped code that fits cannot enter focus or pan. | `phase0-core`: `test_wide_block_focus` checks genuine wide-code panning and then verifies that wrapped code uses the ordinary Enter behavior and rejects Pan Right. |
| 13 | **P2 — Overlay shortcuts use inconsistent switching rules** | Primary overlay shortcuts replace another primary overlay, invoking the active overlay shortcut closes it, and nested font/document/message pickers consume unrelated shortcuts. Background bookmark, next-result, and font-size changes remain blocked. The complete transition table is published in the user guide. | `phase0-core`: modal switching/consumption sequences in `test_modal_input_isolation_and_escape_order` and `test_ctrl_escape_quits_through_overlays`. See [Ctrl-shortcut routing](USER_GUIDE.md#54-ctrl-shortcut-routing-while-an-overlay-is-open). |
| 14 | **P2 — Enter always activates the first link in a block** | A block with multiple links opens a visible keyboard-navigable chooser. Up/Down selects a target and Enter activates that selection. Single-link blocks retain direct activation. | `phase0-core`: `test_link_navigation_and_requests` checks chooser entry and `test_multiple_links_are_keyboard_selectable` selects and opens the second of three links. |
| 15 | **P2 — State rejection and save failures are silent** | Corrupt/stale/unreadable state produces a one-time `Saved state warning`. Atomic-write failure produces `State not saved` after an immediate settings/bookmark save attempt and before normal Esc exit. `Ctrl+Esc` still attempts a final save but does not pause for a dialog. | `phase0-core`: `test_corrupt_state_is_reported_once`, `test_state_save_failure_is_visible_after_setting_change`, and `test_state_save_failure_is_visible_before_normal_exit`. |
| 16 | **P2 — Empty, binary-like, and unreadable documents recover inconsistently** | Empty UTF-8 Markdown opens an explicit empty-document state. Binary-signature/control-heavy input is rejected with a visible error. Document replacement remains transactional: a failed in-document open keeps the current document and reports the original reason. | `phase0-core`: `test_empty_document_has_an_explicit_state` and `test_binary_direct_open_has_visible_error_state`, with file-error and browser lifecycle coverage in the Phase 0 harness tests. |

The resolutions intentionally do not turn the recursive browser into a folder
tree, add an inline link cursor, add a full calculator IME, or implement
right-to-left paragraph ordering. Those are documented product-scope boundaries
in [User guide section 14](USER_GUIDE.md#14-remaining-scope-boundaries), not
unresolved regressions from the dated audit.

## Exact validation run

The documentation reconciliation ran the existing desktop test binaries with:

```sh
ctest --test-dir build/desktop --output-on-failure
```

Result on 2026-07-14: **16/16 tests passed, 0 failed**.

| CTest target | Result |
|---|---|
| `phase0-core` | Passed |
| `phase1-utf8` | Passed |
| `phase5-unicode` | Passed |
| `phase1-font-pack` | Passed |
| `phase1-text` | Passed |
| `phase2-markdown` | Passed |
| `phase2-fenwick` | Passed |
| `phase2-block-layout` | Passed |
| `phase2-state` | Passed |
| `phase4-search` | Passed |
| `phase3-math-parser` | Passed |
| `phase3-math-layout` | Passed |
| `phase3-math-golden` | Passed |
| `phase0-desktop-harness` | Passed |
| `phase0-direct-md-browser` | Passed |
| `phase4-raw-markdown-harness` | Passed |

The final Ndless target was rebuilt after the page-layout regression fix:

- `build/ndless/nmarkdown.tns`: **1,611,364 bytes**
- SHA-256: `7bbd069f20ff0bae894e0ffebc2c344ff68d77ff1f9ad8ad1aba08d6d5954dd4`
- Zehn executable payload: 1,598,632 bytes
- Required load memory: 2,859,744 bytes
- Flags: compressed, `lcd_blit`, and HW-W 240 × 320 support

The supplied `markdown-formula.md.tns` was checked with the final font stack:
**230 formula instances / 204 unique formulas**, with 0 parser errors, 0 layout
errors, and 0 replacement glyphs.

## Post-fix Firebird and screenshot evidence

The final native ARM build was cold-booted with the PocketJS-NSpire/Firebird
harness at pinned revision `556ca6740ce1`. All eight exact fixtures passed,
including post-PASS liveness and reviewed RGB565 framebuffer assertions:

| Fixture | Covered behavior | Result |
|---|---|---|
| Baseline reader | document load, ordinary Markdown, Scroll chrome | Passed |
| Formats | DejaVu body/italic, DejaVu Mono code, loaded Sarasa CJK, inline/display math | Passed |
| Math | radical, fraction, script, matrix, vector, cases and anti-aliasing endpoints | Passed |
| Complex math | equal-width plus/minus strokes, nested radical, aligned fractions | Passed |
| Page Swipe | one Down key reaches complete page 2 of 3, not the final page | Passed |
| TOC | two forward jumps, one backward jump, reopen/cancel liveness | Passed |
| Startup cancel | Esc leaves the startup browser and returns to TI-OS | Passed |
| Theme | open Settings → Dark → close → scroll → reopen | Passed |

Current post-fix native screenshots are available as:

- `artifacts/firebird-final-formats.png`
- `artifacts/firebird-final-math-review.png`
- `artifacts/firebird-final-page-swipe.png`
- `artifacts/firebird-final-theme.png`
- `artifacts/firebird-final-contact.png` (2 × 2 review sheet)

The original audit's screenshot set remains historical pre-fix evidence and is
not relabeled. The files above are a separate post-fix evidence set rendered at
the calculator's native 320 × 240 resolution.

## Physical-hardware verification still required

The following checks cannot be closed by semantic desktop tests or by Firebird
alone. They must be run on real supported calculators, preferably both an
original CX and a CX II:

1. **Exit and chord routing:** `Ctrl+Esc` from the document and every overlay,
   ordinary Esc back-stack order, `Ctrl+T`, and Plus/Minus on the physical
   keypad matrix.
2. **Modal isolation under real input:** page keys, held-key repeat, simultaneous
   modifiers, touchpad drags, and horizontal swipes while each overlay or nested
   picker is visible.
3. **Search entry:** mapped letters/digits/space/period, Shift case, Delete
   repeat, query-limit behavior, result navigation, and `Ctrl+N` on the actual
   keypad adapter.
4. **Filesystem behavior:** adjacent `.nmdstate` creation, atomic replacement,
   corrupt-state recovery, read-only directories, permission errors, and
   recovery after a failed save on the calculator filesystem.
5. **Transferred fonts:** valid optional CJK selection plus missing, oversized,
   unsupported-outline, and disappearing-after-list files under real TI
   transfer naming and device memory limits.
6. **Physical LCD review:** Body, Monospace, mixed Simplified Chinese/Japanese,
   italics, code, inline/display math, selection colors, Standard/High contrast,
   and 9–10 px secondary labels in Light and Dark themes.
7. **Performance and memory:** long documents, large formula galleries, glyph
   atlas churn, repeated reflow, browser caps, and optional CJK use under real
   CX and CX II RAM/timing constraints.
8. **Touch thresholds:** accidental-scroll resistance, swipe direction, page
   granularity, and key-versus-touch priority on each hardware generation.

Firebird remains the correct native ARM/framebuffer gate before this physical
matrix, but a Firebird pass must not be presented as proof of LCD legibility,
key-matrix timing, touchpad thresholds, TI filesystem permissions, or memory
headroom on a real calculator.
