# nMarkdown UI/UX audit completion record — 2026-07-14

> **Checkpoint notice (2026-07-15):** This record predates streamed external
> font files, the 0–10 Text sharpness setting, and the CX II 20 MiB on-disk
> font admission limit. Its retained-buffer and package-identity statements
> are historical. See [USER_GUIDE.md](USER_GUIDE.md) and
> [MEMORY_PROFILE.md](MEMORY_PROFILE.md) for the current behavior.

This document was the post-implementation record for the findings in
the [dated audit](UI_UX_AUDIT_2026-07-14.md). The audit and its
[screenshot index](UI_UX_AUDIT_EVIDENCE_2026-07-14.md) remain historical
evidence of the earlier build; their screenshots and verdict must not be read
as the state of the tree described here.

## Status at this checkpoint

| Gate | Current result | What the result proves |
|---|---|---|
| Source review | Implemented for all 16 recorded P1/P2 findings | The current code contains a resolution for each finding. |
| Desktop CTest | **21/21 passed, 0 failed** in both ordinary and ASan/UBSan builds on 2026-07-15 | The semantic, layout, parser, raster, state, menu-layout, rename-fallback, Ndless-input, and desktop-harness regressions listed below pass on the host. |
| Current Ndless package | **Built and identified**: 1,627,198 bytes; SHA-256 `b982324a92012edebe678ffce5d40fa8b1a575e506c7c17e87d40509a42a9d9d` | The identity and `genzehn --info` fields below were measured from the current `build/ndless/nmarkdown.tns`. |
| PocketJS-NSpire / Firebird | **8 targeted fixtures passed**: state persistence, repeated CJK font-menu navigation, the theme/settings cycle, oversized aligned-formula focus/pan, reversed horizontal swipes, the reader keymap including Search digit ownership, dotted bold-italic `\imath`/`\jmath`, and the six-page math-symbol gallery | The current math fixtures pin annotation-column and tag-lane geometry, symbol coverage including `\textvisiblespace`, glyph weight, visible dots, accented forms, and stable 320 x 240 framebuffers. The keymap fixture also proves that numeric aliases enter text in Search and resume reader navigation immediately after Esc. The prior interaction fixtures retain their narrower evidence. |
| Physical CX / CX II | **Not run** | Real keypad, touchpad, LCD, filesystem, latency, and RAM behavior remain unverified. |

The host implementation is therefore in a resolved state, and the current
native package has an identified build plus eight targeted Firebird passes. This
record does not declare a physical-calculator release pass. Firebird is a
native ARM/framebuffer gate, not a substitute for the hardware matrix at the
end of this document.

## Current font and rendering baseline

- **Body:** embedded DejaVu Sans.
- **Monospace:** embedded DejaVu Sans Mono.
- **CJK:** no face is selected by default. The supplied optional file is
  `build/fonts/SarasaFixedSC-Regular.ttf.tns`, a 6,105,504-byte Sarasa Fixed SC
  Regular subset selected through Settings → Fonts → CJK.
- **Math:** embedded Latin Modern Math with OpenType MATH data; formula symbols
  and Latin annotations do not use the Body or Monospace role picker. CJK
  inside `\text{...}` can use the selected CJK fallback. `\imath` and
  `\jmath` intentionally map to that face's native dotted bold-italic i and j
  glyphs rather than upright or dotless symbols.

The reader asks for the semantic role first, shapes OpenType runs with
HarfBuzz, rasterizes outlines with the minimal FreeType build, and only falls
through to another role when the requested face lacks a glyph. This prevents a
CJK face from taking over Latin prose merely because it also maps Latin.

When a document contains CJK and no CJK role is selected, the reader shows the
one-time message `CJK font needed` with a direct path to the Sarasa selector.
Body and Monospace remain usable without any separately transferred font.

### What “no font license in the pack” means

The generated core-font manifest sets `embed_license: false`; nMarkdown does
not append or display a separate font-license text payload in the runtime font
pack or font-selection UI. The repository still carries the upstream notices
required for source and binary redistribution in
[`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).

Transferred `.ttf`/`.otf` data remains font data. This record does **not** claim
that upstream copyright or license records inside an OpenType font's own name
tables have been stripped. Those records are distinct from the extra pack-level
license field disabled above.

## Implemented resolutions

| # | Audit finding | Current resolution | Current regression evidence |
|---:|---|---|---|
| 1 | Inline math overlaps prose | Inline math now contributes its real advance and vertical metrics during wrapping and bounded word-space compression. The regression checks non-overlap and monotonic placement at every supported body size from 12 through 22 px. | `phase2-block-layout`: `test_inline_math_never_overlaps_following_prose`, inline/display integration, and automatic line spacing; `phase3-math-*`. |
| 2 | Visible overlays leak input to the document | Input is routed to the visible top layer before reader actions. Page movement, pointer motion, font-size changes, bookmarks, search-next, activation, and theme changes are consumed by modal layers. `Ctrl+Esc` remains globally highest priority. | `phase0-viewer-interactions` exercises the shortcut/isolation matrix in both Scroll and Page Swipe modes; `phase0-core` retains focused cases. |
| 3 | Empty Bookmarks can activate an invisible TOC row | Bookmark and TOC selection are isolated. An empty bookmark tab has no activatable row, and Enter is a consumed no-op. | `phase0-core`: `test_toc_bookmark_empty_state_is_safe`. |
| 4 | A failed direct launch silently shows demo content | Direct-open failure stays in a named error state and offers Scratchpad/`Ctrl+O` to choose another file or Esc to exit. It does not replace the requested file with a demonstration document. | `phase0-core`: direct wrapped-Markdown and `test_binary_direct_open_has_visible_error_state`. |
| 5 | Restored Page Swipe can show a blank final page | Restored source anchors are resolved to a content-bearing page and clamped after lazy layout growth. | `phase0-core`: `test_page_swipe_restore_has_visible_content` and `test_page_swipe_lazy_growth_does_not_skip_a_page`. |
| 6 | Esc clears hidden wide focus before visible Settings | Back closes the visible/nested layer first, then wide focus, then the document. The covered block remains focused when Settings is dismissed. | `phase0-viewer-interactions`: full Back-stack matrix; `phase0-core`: modal Escape ordering. |
| 7 | Reader Controls behaves like a hidden command surface | Reader Controls is passive and modal. Doc or Esc closes it; reader movement and activation do not pass through it. | `phase0-viewer-interactions`: passive overlay isolation in both reading modes. |
| 8 | Contrast changes while the visible label stays stale | Settings rows are rebuilt in the same frame when contrast or theme changes. Selection is now marked with visible geometry in addition to color. | `phase0-core`: settings repaint responsiveness; current desktop Settings frame listed below. Text OCR is not part of the automated assertion. |
| 9 | Oversized font becomes a false missing-file error | `.tns` fallback is attempted only for a missing path. Decode/size failures retain their specific font error and the previous role face. Per-file and aggregate retained-font limits are checked before loading. | `phase0-core`: `test_font_size_error_is_not_overwritten_by_retry`, `test_aggregate_external_font_limit_is_enforced_before_read`, and font-menu rollback. |
| 10 | Search exposes raw Markdown and clips context | Snippets remove common Markdown punctuation, include heading context, and truncate at UTF-8/word-safe boundaries with ellipses. | `phase4-search`: reader-facing snippets, UTF-8 limits, matching modes, and Unicode folding. |
| 11 | Document browser is ambiguous and silently capped | Duplicate basenames receive distinguishing parent suffixes. A real document overflow adds the visible, non-selectable `More documents not shown` row. Font discovery separately inspects at most 2,048 filesystem entries, returns at most 128 fonts, processes each directory's files before descending, and ends an incomplete result with the non-selectable `Font search stopped early` row. The picker stores labels but shapes only its six visible rows. | `phase0-core`: duplicate labels, exact-cap/truncated-list behavior, the 2,200-entry root-first scan-budget case, full 6,105,504-byte Sarasa retention with a 128-row responsive picker, and one cached application-level listing with no font reads while browsing. |
| 12 | Wrapped code falsely accepts wide focus | Wide focus is admitted only for measured post-layout horizontal overflow. Wrapped code that fits cannot pan. | `phase0-core`: `test_wide_block_focus`. |
| 13 | Overlay shortcuts switch inconsistently | Primary shortcuts follow one replacement/close policy; nested font/document/message layers consume unrelated shortcuts. | `phase0-viewer-interactions`: Quit through 12 overlay states, Back stacks, and non-owning shortcut isolation. |
| 14 | Enter always activates the first link | Multiple links open a keyboard-selectable chooser; Up/Down and Enter select the intended target. A single link remains direct. | `phase0-core`: link request coverage and `test_multiple_links_are_keyboard_selectable`. |
| 15 | State rejection and save failure are silent | Rejected state produces a one-time warning. Failed writes produce `State not saved` on ordinary interaction/exit while immediate `Ctrl+Esc` remains immediate. Esc or Enter dismisses an exit-time warning and completes the pending exit. If Ndless reports rename as unimplemented/unsupported, replacement falls back to a staged destination rewrite followed by a complete byte comparison before the temporary copy is removed. State errors say `state file`, not `document`. | `phase0-core`: corrupt-state, save-failure paths, two-key Esc/Enter completion, and immediate Ctrl+Esc; `phase0-stdio-rename-fallback`: creation and replacement under simulated `ENOSYS`. |
| 16 | Empty, binary-like, and unreadable documents recover inconsistently | Empty Markdown has an explicit empty state; binary/control-heavy input is rejected; failed in-document replacement keeps the current document. Malformed UTF-8 expansion is preflighted against the source budget. | `phase0-core`: empty, binary direct-open, transactional open, and `test_malformed_utf8_cannot_expand_past_source_budget`. |

## Additional post-audit hardening

- Long filenames are UTF-8 ellipsized so the separate top-right Page Swipe
  counter remains readable. Scroll mode has no page counter.
- The progress indicator is a full-width 1–2 px line at the very top. The old
  side rails, footer bar, and wide-focus cursor-like bottom artifact are gone;
  at least 2 px remains below document ink.
- Reading progress now has mode-specific endpoints. Scroll mode uses the
  continuous scroll offset over the maximum scroll offset; Page Swipe uses the
  current page's zero-based position over the final page's zero-based position.
  Both modes are exactly 0% at the beginning and exactly 100% at the end. A
  one-viewport document remains at 0%; a full-width red strip is reserved for a
  document-load error rather than representing reading position.
- Page Swipe suppresses its page counter when no document is loaded or a load
  error is visible, so the red status strip is never paired with a synthetic
  `1 / n` reading position.
- Settings and modal panels dim the document beneath them, and selected rows
  use high-contrast geometry instead of relying on color alone.
- Large document/font operations present `Opening document` or `Loading font`
  before synchronous work starts.
- Menu typography is internally consistent and larger: titles and ordinary
  list rows are 13 px, compact Settings rows are 12 px, auxiliary text is
  11 px, and the Search query is 14 px. Shaping and drawing use the same size;
  `phase0-menu-layout` also checks that the final rows fit the 320 x 240 panels.
- `\imath` and `\jmath` use the native dotted bold-italic i and j glyphs
  provided by Latin Modern Math, with parser, layout, and golden-raster
  coverage including accents.
- CJK discovery is bounded independently of font loading: files placed directly
  in My Documents are considered before nested folders, a truncated scan is
  visible, and only the six currently visible filename rows are shaped.
- State replacement retains rename-based atomicity where available. The
  Ndless `ENOSYS`/unsupported-rename fallback keeps the completed temporary
  payload until a staged destination rewrite passes a byte-for-byte check.
- External font replacement is transactional and moves owned buffers instead
  of retaining avoidable copies. The sum of retained external role fonts is
  capped as well as each individual input.
- Raw document bytes are released after parse/identity work. Invalid UTF-8 is
  rejected before replacement-character expansion can exceed the configured
  source budget.
- Generated font/math sources have explicit build dependencies, and Ndless
  objects depend on a build-flag stamp so changing native flags cannot silently
  reuse incompatible objects.
- Document-level Enter/touchpad click is inert when no link or overflowing
  block is actionable; theme changes are confined to Reader Settings. Tab/1
  maps to Page Down, 7 to Page Up, 2/4/6/8 to Down/Left/Right/Up, and Scratchpad
  to the document browser. Equivalent physical aliases retain distinct edge
  identities and repeat timing, while explicit Esc/Ctrl chords keep priority.

## Desktop validation and current visual checks

The following command was run from the current tree on 2026-07-14:

```sh
ctest --test-dir build/desktop --output-on-failure
```

Result: **21/21 passed, 0 failed**. The same 21 tests also passed under the
configured ASan/UBSan build.

| Test target | Result |
|---|---|
| `phase0-core` | Passed |
| `phase0-viewer-interactions` | Passed |
| `phase0-input-ndless-liveness` | Passed |
| `phase0-clock-ndless-bounded` | Passed |
| `phase0-menu-layout` | Passed |
| `phase0-stdio-rename-fallback` | Passed |
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

These current 320 × 240 desktop frames are visual spot checks, not native or
physical-calculator evidence:

- [Inline math at 18 px](../artifacts/desktop-inline-18-fixed.png)
- [Inline math at 22 px](../artifacts/desktop-inline-22-fixed.png)
- [DejaVu Body/Monospace with loaded Sarasa CJK](../artifacts/latest-cjk.png)
- [Missing-CJK guidance dialog](../artifacts/latest-cjk-prompt.png)
- [Long title with separate page counter](../artifacts/latest-long-title.png)
- [Settings scrim and selection marker](../artifacts/latest-settings.png)
- [Dotted bold-italic `\imath`/`\jmath` desktop review](../artifacts/imath-jmath-dotted-bold-italic-desktop.png)
- [Firebird dotted bold-italic `\imath`/`\jmath` and accented forms](../artifacts/imath-jmath-bold-italic-firebird.png)
- [Bold-italic math, Latin Modern annotation text, and CJK fallback](../artifacts/imath-jmath-bold-and-formula-text.png)
- [Exact `markdown-formula.md` alignment after a viewport pan](../artifacts/markdown-formula-align-panned.png)
- [Firebird oversized formula with aligned annotation columns](../artifacts/final-align-annotation-columns-firebird-2x.png)
- [Firebird six-page math-symbol gallery](../artifacts/math-symbol-gallery-firebird-contact-sheet.png)
- [Firebird Scroll-mode horizontal-swipe result](../artifacts/scroll-swipe-firebird.png)
- [Current 12 px Settings typography](../build/menu-settings-after.png)
- [Current Reader Controls keymap](../artifacts/keymap-reader-controls.png)
- [Firebird Tab Page Down](../artifacts/firebird-keymap-tab-page-down.png)
- [Firebird Down baseline](../artifacts/firebird-keymap-down-baseline.png)
- [Firebird inert Enter/click followed by physical `2`](../artifacts/firebird-keymap-enter-click-n2-liveness.png)
- [Firebird Search digits followed by Esc and physical `2`](../artifacts/firebird-search-digits-esc-n2.png)

The 347 frames indexed in the historical evidence document and the earlier
306/306 scripted capture manifest are retained as earlier-checkpoint evidence.
They are not silently relabeled as post-fix screenshots; the current links
identify their individual desktop or Firebird checkpoints.

## Native package and targeted Firebird results

The following identities were measured from the files in the current build
tree. The Firebird rows are dedicated integration builds with fixture-only
input setup; they are evidence for the stated interaction, not a claim that
every historical fixture ran against the ordinary package below.

| Artifact | Bytes | SHA-256 | Native metadata |
|---|---:|---|---|
| `build/ndless/nmarkdown.tns` | 1,627,198 | `b982324a92012edebe678ffce5d40fa8b1a575e506c7c17e87d40509a42a9d9d` | Zehn header `0x31bc`; 6,785 relocations; 11 flags; 36 extra bytes; 2,889,188 load bytes; 1,614,466 Zehn executable bytes; entry `0x0`; compressed type 0; application `nMarkdown`; author `nMarkdown contributors`; version 1; `lcd_blit`; HW-W 240 x 320 |
| `build/ndless-firebird-imath/nmarkdown-firebird-imath.tns` | 1,618,507 | `c8e151584176ef7d4f6c51abb2d8198275cf2ce4faa495a22f88dfeac13fbf84` | Dedicated dotted bold-italic i/j package; Zehn header `0x31bc`; 6,005 relocations; 11 flags; 60 extra bytes; 2,871,764 load bytes; 1,605,775 Zehn executable bytes; application `nMarkdown-Firebird-Bold-Italic-IJ` |
| `build/ndless-firebird-oversized-formula/nmarkdown-firebird-oversized-formula.tns` | 1,628,372 | `3db2b7b9ff82457c8bee0f8eb392826bd5389f5a839d5450b6e0218e67ed46fa` | Dedicated exact-formula integration package; Zehn header `0x31bc`; 6,856 relocations; 11 flags; 60 extra bytes; 2,891,368 load bytes; 1,615,640 Zehn executable bytes; application `nMarkdown-Firebird-Oversized-Formula` |
| `build/ndless-firebird-symbols/nmarkdown-firebird-symbols.tns` | 1,629,023 | `b3d36d830d7a7bafcb459d36aeb62147cc1e36a611dd4b8f796921ee1fc3d55f` | Dedicated six-page math-symbol package; Zehn header `0x31bc`; 6,830 relocations; 11 flags; 56 extra bytes; 2,892,444 load bytes; 1,616,291 Zehn executable bytes; application `nMarkdown-Firebird-Math-Symbols` |
| `build/ndless-firebird-scroll-swipe/nmarkdown-firebird-scroll-swipe.tns` | 1,619,602 | `b0f14ddcc94a692d17810326c0df55cfbb125ea3f45272e82a681dd01fcba09d` | Dedicated touch/progress package; 6,045 relocations; 56 extra bytes; 2,874,072 load bytes; 1,606,870 Zehn executable bytes; application `nMarkdown-Firebird-Scroll-Swipe` |
| `build/ndless-firebird-page/nmarkdown-firebird-page.tns` | 1,618,989 | `87692b27fb181acb9b8ae160388cab06efc47f1094ff6af42d00746eb84c464a` | Dedicated Page Swipe/progress package; 6,009 relocations; 48 extra bytes; 2,872,544 load bytes; 1,606,257 Zehn executable bytes; application `nMarkdown-Firebird-Page` |
| `build/ndless-firebird-keymap/nmarkdown-firebird-keymap.tns` | 1,628,992 | `bd51926d8eed88bbe0a442ad62f62265d4b2cad72ef0196fe1c7f3c9c335a129` | Dedicated reader/Search-key package; Zehn header `0x31bc`; 6,869 relocations; 11 flags; 52 extra bytes; 2,892,748 load bytes; 1,616,260 Zehn executable bytes; application `nMarkdown-Firebird-Keymap` |
| `build/fonts/SarasaFixedSC-Regular.ttf.tns` | 6,105,504 | `e2a9016b9bfd543945f53e5e89867005febe85f4087035a4590ba2906e9726db` | Optional CJK transfer file |

Run and review each current native fixture separately. A serial marker without
the expected stable framebuffer is not a full framebuffer pass.

| Fixture | Current-tree result | Reviewed screenshot/result |
|---|---|---|
| State creation/replacement and exit | **Passed**: stable framebuffer 3/3, 0 mismatched expected pixels; verifier required at least two `STATE_SAVE_OK` markers, no `STATE_SAVE_FAIL`, and `EXIT_OK` | [result](../build/firebird-state/result.json), [captured RGB565 frame](../build/firebird-state/frame.ppm) |
| CJK font list repeated open/move/back | **Passed**: list opened and selection moved twice across 18 presented checkpoints; stable framebuffer 3/3, 0 mismatched expected pixels | [result](../build/firebird-font-menu/result.json), [screenshot](../build/firebird-font-menu/screenshot.png), [serial evidence](../build/firebird-font-menu/serial-evidence.txt) |
| Theme/settings cycle | **Passed**: current native build toggled Dark, closed, scrolled, and reopened Settings; stable framebuffer 3/3 with all 9 expected pixels matching | [result](../build/firebird-theme-current/result.json), [screenshot](../artifacts/firebird-theme-current.png) |
| Oversized aligned formula | **Passed**: all three `=` runs share one x coordinate, and `Given`, `additive identity`, and `equations (1) and (2)` share one annotation-column right edge. The `(1)`/`(2)` equation tags occupy a separate lane and do not shift the untagged third row. The native interaction reached the exact right-pan clamp (`29` px), stayed responsive after fine pan and swipe, and produced a stable framebuffer 3/3 with all 16 expected pixels matching, 0 mismatches, and frame hash `a50e559a3b59c117`. | [result](../artifacts/oversized-align-annotation-columns-firebird-result.json), [captured RGB frame](../build/firebird-oversized-formula/frame.ppm), [review screenshot](../artifacts/final-align-annotation-columns-firebird-2x.png) |
| Math-symbol gallery | **Passed on all 6 pages**: each schema-13 capture reached stable framebuffer 3/3 with 0 mismatches. The reviewed page-2 screenshot visually confirms that direct `\textvisiblespace` and canonical `\text{A\textvisiblespace B}` both render Latin Modern Math's U+2423 open-box glyph instead of literal command text or an error box; the host golden test separately pins that raster output. The other pages cover the requested aliases, Greek and binary symbols, relations and negation, arrows and large operators, accents, and tall delimiters. Frame hashes are `dc63b05b6b1e4a8f`, `36dedc994df8acd5`, `65968187563da2d8`, `bd8d8af6db953f71`, `59ae80471a428a89`, and `ae76f2479165b31e`. | [combined result](../build/firebird-symbols/summary.json), [visible-space screenshot](../artifacts/math-symbol-gallery-firebird-page-02.png), [contact sheet](../artifacts/math-symbol-gallery-firebird-contact-sheet.png), [source fixture](../samples/math-symbol-gallery.md) |
| Scroll horizontal swipes | **Passed**: Down established `0 → 18`; a raw left swipe moved toward earlier content `18 → 0`; a new-contact right swipe moved toward later content `0 → 196`; and a final Down remained live at `196 → 214`. Every transition rendered and presented; stable framebuffer 3/3, 0 mismatched pixels, frame hash `993bded4d283543e`. | [result](../build/firebird-scroll-swipe/result.json), [captured RGB frame](../build/firebird-scroll-swipe/frame.ppm), [review screenshot](../artifacts/scroll-swipe-firebird.png) |
| Scroll/Page Swipe reading progress | **Passed in six cold-boot captures**, each stable 3/3 with 0 mismatched pixels. Scroll produced 0 px at `0/722`, 7 px at `18/726`, and 320 px at `917/917`. Page Swipe produced 0 px at its first page, exactly 160 px at page 2/3, and 320 px at page 3/3. The verifier independently recomputed each formula from a post-layout native trace and matched both RGB565 rows. | [combined result](../build/firebird-progress/result.json); Scroll [start](../artifacts/progress-scroll-start-firebird.png), [after Down](../artifacts/progress-scroll-after-down-firebird.png), [end](../artifacts/progress-scroll-end-firebird.png); Page Swipe [start](../artifacts/progress-page-start-firebird.png), [2 / 3](../artifacts/progress-page-2-of-3-firebird.png), [end](../artifacts/progress-page-end-firebird.png) |
| Reader and Search keymap | **Passed in four cold-boot runs**: Tab emitted Page Down `0 → 196`; real Down emitted line down `196 → 214`; ordinary Enter and an exact native click report emitted no scroll transition; physical `n2` then emitted the identical `196 → 214` line movement. In Search, native `2/4/6/7/8` produced the exact query sequence `2 → 24 → 246 → 2467 → 24678` with no scroll transition; after Esc, physical `2` immediately resumed line navigation `196 → 214`. PocketJS cannot inject physical `1`, so the fake Ndless matrix and Viewer regression pin all six aliases including `1`. Every native run reached stable framebuffer 3/3 with 0 mismatched pixels. The Down, Enter/click/2, and Search/Esc/2 PPMs are byte-identical, SHA-256 `01765c3253e4b7cfb82d86ac6e92c57633f5ddafb70b19d05b882f316cf0d253`, frame hash `962c63a55133103e`. | [Tab result](../build/firebird-keymap/tab-page-down/result.json), [Down result](../build/firebird-keymap/down-baseline/result.json), [Enter/click/2 result](../build/firebird-keymap/enter-click-n2-liveness/result.json), [Search digits/Esc/2 result](../build/firebird-keymap/search-navigation-digits-are-text/result.json), [review screenshot](../artifacts/firebird-search-digits-esc-n2.png) |
| Dotted bold-italic math letters | **Passed**: stable framebuffer 3/3 with 0 mismatched pixels, frame hash `7c1f87ab61df8361`. Native region checks found visible dots on both command glyphs, heavier ink than ordinary italic (`i`: 14 → 22, `j`: 21 → 29), and complete hat/vector accented forms. | [result](../artifacts/imath-jmath-bold-italic-firebird-result.json), [detailed evidence](../artifacts/imath-jmath-bold-italic-firebird-summary.json), [review screenshot](../artifacts/imath-jmath-bold-italic-firebird.png) |
| Baseline reader | **Not rerun at this checkpoint** | Older artifact not promoted as current evidence |
| Formats / font roles | **Not rerun at this checkpoint** | Older artifact not promoted as current evidence |
| Math | **Not rerun at this checkpoint** | Older artifact not promoted as current evidence |
| Complex math review | **Not rerun at this checkpoint** | Older artifact not promoted as current evidence |
| Page Swipe | **Passed**: one native Up reached page 2/3; the reviewed boundary is green through x=159 and neutral at x=160, with the page label/content pixels also pinned. Stable framebuffer 3/3, all 16 expected pixels matched, frame hash `00b88156226aacd8`. | [result](../build/firebird-page/result.json), [screenshot](../artifacts/progress-page-2-of-3-firebird.png) |
| Repeated TOC jumps | **Not rerun at this checkpoint** | Older artifact not promoted as current evidence |
| Startup browser cancel | **Not rerun at this checkpoint** | Older artifact not promoted as current evidence |

### CJK fixture limitations

The current font-menu fixture creates a small Sarasa-compatible file and
deliberately does **not** load it. It proves that the app can enter the CJK file
list, move selection, backtrack, and enter/move again without freezing or
reading the font payload. It does not prove full-size Sarasa load time or memory
headroom.

The older Firebird Formats fixture embeds a **17,004-byte, sample-specific
Sarasa-compatible mini font** generated from only the fixture text. Its earlier
run exercised role routing, native font parsing, shaping, rasterization, and
mixed-font composition for that older tree; it was not rerun at this checkpoint
and is not current-tree evidence. Even when rerun, the mini font would not prove
that a calculator can load, retain, reflow, and repeatedly render the actual
6,105,504-byte transferable Sarasa file within acceptable time and memory.

The production Ndless limits are 6 MiB total retained external fonts on the
original CX and 12 MiB on CX II. The supplied Sarasa file is below the 6 MiB
byte limit, but its FreeType/HarfBuzz/runtime overhead and glyph-cache pressure
still require device measurement.

### Keymap fixture limitations

The pinned PocketJS tape can inject physical Tab and `n2`, but it has no action
for `1`, `4`, `6`, `7`, `8`, or Scratchpad. Those production matrix constants,
semantic mappings, direct equivalent-key transitions, modifier priority, repeat
timing, and Scratchpad non-repeat behavior pass the raw Ndless host regression.
They still need a real-keypad pass. PocketJS also lacks a touchpad-click action;
the fixture transparently converts its first post-Enter center contact into the
exact `TPAD_ARROW_CLICK` report consumed by `InputNdless`. That proves the
native click-report path but is not a physical touchpad-button claim.

## Evidence that cannot currently be reproduced

The user-supplied file
`/Users/ryougi/Downloads/markdown-formula.md.tns` is not present at this
checkpoint. The older resolution's formula-instance/error counts have no
retained source hash and probe output that ties them to this final tree, so they
must not be repeated as current proof. Reattach or restore the exact file, hash
it, then record parser, layout, replacement-glyph, and screenshot results.

## Budget reconciliation

The early plan's size figures are design targets, not the limits currently
enforced by the implementation:

- `assets/core.fpk` is 1,831,016 bytes, above the old “≤1.5 MiB” target.
- the transferable Sarasa subset is 6,105,504 bytes, above the old “≤4 MiB”
  optional-fallback target but below the production CX 6 MiB file/aggregate
  limit.
- the old “≤12 MiB steady state” target has not been measured on either
  calculator generation.

The plan should be revised to distinguish aspirational package/steady-state
budgets from enforced input limits. Passing a byte-limit check is not proof of
runtime memory headroom.

## Physical-hardware verification still required

Run the following on both an original CX and a CX II before calling the release
hardware-complete:

1. `Ctrl+Esc` from the document and every top-level/nested overlay; ordinary
   Esc through each back stack; held-key and simultaneous-modifier priority;
   physical Tab/1/7, 2/4/6/8, Scratchpad, and direct transitions between an
   arrow and its numeric alias.
2. Repeated TOC/header jumps, including the second jump; startup browser Esc;
   in-document browser cancel; Scroll/Page Swipe switching and restoration.
3. Horizontal swipe direction in both reading modes, vertical drag, diagonal rejection,
   touchpad click, and isolation of all gestures while a modal is visible.
4. Real `.md`, `.markdown`, `.md.tns`, and `.markdown.tns` file association and
   TI-OS launch behavior. Registration in code does not prove every OS/version
   integration path.
5. Adjacent state-file creation, rename-based atomic replacement when supported,
   the byte-verified fallback when rename is unavailable, read-only directories,
   permission errors, corrupt-state recovery, and failed-save messaging on the
   calculator filesystem.
6. Transfer and select the actual 6,105,504-byte Sarasa file; measure load time,
   peak/steady memory, reflow time, repeated document switching, and glyph-cache
   churn on both models.
7. Inspect DejaVu Sans prose, DejaVu Sans Mono code, Sarasa Simplified Chinese
   and Japanese, italics, inline/display math, 11–14 px overlay typography,
   selection markers, and both contrast modes on the physical 320 × 240 LCD.
8. Exercise maximum-size documents, invalid UTF-8, formulas, browser caps, and
   aggregate external-font limits while monitoring liveness and memory.

Firebird may hide cache-coherency, key-matrix, filesystem, LCD, and performance
problems. Preserve the normal cache-cleaned `lcd_blit` path for hardware; do not
treat simulator-only behavior as a hardware workaround.

## Release interpretation

The 16 audit findings have source-level resolutions and passing desktop
regressions. The current ordinary Ndless package is identified, and eight
targeted native fixtures pass, including state persistence, CJK-menu liveness,
theme/settings, oversized formulas, Scroll swipes, the supported physical
reader-key subset, dotted bold-italic math letters, and the six-page
math-symbol gallery.
Final release confidence still depends on:

1. rerunning and visually reviewing the seven remaining broader Firebird
   fixtures on the current tree,
2. restoring the user formula corpus if its exact coverage is required, and
3. completing the physical CX/CX II matrix above.

Until those gates are recorded, describe the state as **host-resolved with
targeted native verification; full native regression and hardware verification
pending**, not “all platforms verified.”
