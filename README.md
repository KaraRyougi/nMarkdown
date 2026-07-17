# nMarkdown

nMarkdown is a native, feature-rich Markdown reader in development for the
TI-Nspire CX and CX II families. The implementation follows [`PLAN.md`](PLAN.md)
and keeps the document/rendering core independent of Ndless.

## Documentation

- [User guide](docs/USER_GUIDE.md): opening documents, reading controls,
  search, settings, fonts, persistence, and recovery.
- [Mathematical LaTeX support](docs/MATH_SUPPORT.md): the complete native
  symbol-catalog boundary, structures, accents, variants, delimiters,
  operators and limits, exclusions, and resource limits.
- [UI/UX audit](docs/UI_UX_AUDIT_2026-07-14.md): the dated pre-fix findings,
  severity, reproduction steps, and recommendations.
- [UI/UX audit resolution](docs/UI_UX_AUDIT_RESOLUTION_2026-07-14.md): the
  implemented resolutions, regression coverage, and remaining device checks.
- [Audit evidence](docs/UI_UX_AUDIT_EVIDENCE_2026-07-14.md): scenario results,
  screenshots, and verification backends.
- [Audit execution plan](docs/UI_UX_AUDIT_PLAN.md): complete test matrix and
  acceptance criteria.

## Current implementation: Phase 5 baseline

The portable platform, text, parser, and virtual-layout stack now provide:

- A portable 320 x 240 RGB565 surface and clipped drawing primitives.
- An event-driven reader loop that does not redraw while idle.
- A 153,600-byte retained clean-frame cache for modal UI. Opening, navigating,
  and dismissing menus, loading cards, and file/font browsers copies the clean
  RGB565 base instead of traversing and compositing the document again. Any
  document, theme, font, focus, or layout change invalidates it; ordinary
  scrolling and page turns keep their existing rendering path.
- Animated, checkpoint-driven loading feedback for document/font discovery, large document
  read/decode/parse/layout, and external font loading. Fast operations remain
  transactional with no intermediate flash; slow synchronous work presents a
  compact staged status card before the next blocking chunk, advances it at
  bounded filesystem checkpoints, and clears it on success or error. An
  interactively opened document browser presents this card immediately and
  reuses its session catalog on later opens. Applying Font Manager changes
  also paints the loading card immediately before font loading, role rebinding,
  cache invalidation, and document reflow.
- Semantic scrolling, screen-step navigation, panning, menu, activation, and back events. Up/8
  move toward earlier content and Down/2 toward later content in both reading
  modes; Page Up/7 and Page Down/1/Tab retain the same conventional directions
  and repeat only a line clipped at the page boundary, even while a wide block
  is focused.
- A full-width two-pixel progress track at the top edge. Its fill starts at 0%,
  follows the continuous reading offset in both touchpad modes, and reaches the
  full width only at the exact end. No synthetic page count is displayed.
- A strict UTF-8 decoder that rejects overlong sequences, surrogates, truncated
  input, and values above U+10FFFF while always making forward progress.
- A checksummed, bounds-checked private format for the compiled-in core font.
- Embedded printable-ASCII bootstraps: 25,644-byte DejaVu Sans for UI/body
  fallback and 17,492-byte DejaVu Sans Mono for code, both including U+FFFD.
  Task checkboxes use fixed-size rasters ported from GitHub's
  browser-native checkbox, so they add no font payload. The bootstrap keeps
  menus and simple
  ASCII documents usable
  before any external reading font is selected; it is not presented as a full
  document font.
- A size-trimmed FreeType 2.14.3 backend with TrueType/OpenType-CFF loading,
  low-resolution hinting, and grayscale outline rasterization.
- Ordered font fallback with a guaranteed U+FFFD replacement glyph.
- HarfBuzz OpenType shaping over fallback-aware font/script runs, including
  GSUB/GPOS substitutions, kerning, ligatures, mark attachment, and source
  clusters. FreeType rasterizes the shaped glyph IDs into the existing A8
  cache.
- Family-aware external fonts. OpenType metadata automatically associates each
  Reading family with its Regular, Italic/Oblique, Bold, and Bold Italic faces.
  Missing italic and bold faces use FreeType outline oblique and synthetic-bold
  fallbacks; completed bitmaps are never shifted row by row.
- Natural font advances by default, plus an optional one-pixel tracking mode
  that adds space only between shaped clusters. Paragraph-level bidirectional
  ordering remains separate future work.
- A lazily allocated glyph atlas that can grow to 128 256 x 256 A8 pages
  (8 MiB), with cache statistics and LRU page eviction.
- RGB565 alpha compositing with separate light/dark coverage correction LUTs;
  light mode uses a monotonic edge-contrast curve to suppress pale fringe and
  reinforce dark stems, while dark mode lifts mid-coverage edges so small white
  text stays clear on black. Both preserve transparent/opaque endpoints.
- Real anti-aliased Unicode text in the scrollable calculator view, including
  light and dark themes and adjustable text size.
- Strictly capped file loading (4 MiB on CX, 8 MiB on CX II), UTF-8
  sanitization, and MD4C parsing with raw HTML disabled. Plain `.txt` files
  additionally auto-detect UTF-8/BOM, GB2312/GBK, Shift-JIS, EUC-JP,
  and ISO-2022-JP before entering a literal (non-Markdown) layout path.
- CommonMark paragraphs, headings, emphasis, strong text, links, lists,
  blockquotes, code, rules, tables, task lists, strikethrough, and math-span
  tokens in a compact indexed IR.
- The complete semicolon-terminated HTML5 named-entity table plus numeric
  entity recovery.
- Unicode-safe line wrapping with bounded inter-word shrink, list/quote indentation, responsive
  key/value table records, aligned grid tables, and independently pannable
  code blocks.
- Markdown uses lazy visible-region layout over its complete MD4C document IR,
  with a Fenwick height index and bounded LRU block cache. TXT bypasses that IR
  entirely: on Ndless it shows the first screen after a 64 KiB read, then grows
  a reserved 1 MiB sequential byte cache in 32 KiB idle quanta. It retains five
  preceding plus ten following shaped screens. Each admitted HarfBuzz run is
  shaped once and partitioned into those screens at cluster boundaries instead
  of reshaping overlapping prefixes, and next-line/page glyph work runs after
  the requested frame has been presented. A small ring of page-start byte offsets
  supports reverse page movement; TXT progress and saved position use byte
  offsets, and exact total page count is deliberately not calculated.
- Source-position anchors that prevent jumps during lazy measurement and font
  reflow.
- A heading-derived, keyboard-navigable table of contents.
- Checksummed per-document sidecar state with source-relative reading position,
  theme, font size, code-wrap preference, bookmarks, and atomic replacement.
- Exact UTF-8, ASCII-folded, canonical-equivalence, and full Unicode-folded
  search with UTF-8-safe snippets, source-accurate result navigation,
  next-result shortcuts, and glyph-cluster highlights. Markdown searches its
  retained source; TXT performs a sequential scan only while Search is open.
- Source-relative bookmarks with a navigable menu tab and persistence across
  document reflow and reopen.
- Reader settings for theme, high-contrast palettes, font size, automatic
  content-aware line spacing with a manual gap override,
  exact symmetric side margins, responsive/grid tables, code wrap, macOS-style
  Natural/Reversed continuous scrolling, independent reading-order swipe
  direction, and role-based font selection.
- Persisted Vertical Scroll and Horizontal Scroll touchpad modes. Vertical
  Scroll uses continuous vertical drag and horizontal screen-step swipes;
  Horizontal Scroll uses continuous horizontal drag and vertical screen-step
  swipes. Natural continuous scrolling makes up/left advance; Natural discrete
  swiping makes right/down advance. Either direction can be reversed
  independently. Keys remain semantic and never reverse.
- Touch gestures use physical-scale native X/Y deltas, separate tap/start/
  motion deadzones, and a 4:3 dominant-axis lock. One provisional correction
  before a named swipe recovers from landing wobble; after that, diagonal
  jitter cannot redirect the gesture. Slow sub-deadzone movement accumulates.
- Page Up/Down and the discrete swipe axis start on a visual-line top. A line
  clipped by the old viewport is repeated so it becomes fully readable unless
  at least 85% was already visible; a fully or mostly displayed trailing line
  is not carried onto the new screen. An immediate reverse returns to the exact
  prior top. Oversized content still guarantees monotonic progress.
- Full-width 320-pixel paper with no decorative side rails and a guaranteed
  two-pixel text-free inset at the physical bottom edge. Prose wraps against
  the configured margin on both sides; code and tables keep symmetric padding
  inside that same outer content box.
- Explicit wide-block focus for equations, code, and grid tables, with clipped
  local horizontal panning, fine arrow steps, touch panning, and left/right
  continuation indicators. A wrapped code block opens a temporary unwrapped
  pan canvas without changing the document's Wrap setting, then restores the
  prior reading position when focus closes. Enter/click can target an
  overflowing formula anywhere in the visible viewport, and page movement can
  traverse a formula taller than the viewport and continue into following
  content.
- Internal heading links, relative Markdown links, linked-asset
  notices, and view-only external URL dialogs.
- A [bounded mathematical LaTeX engine](docs/MATH_SUPPORT.md) with a
  514-entry native symbol catalog, TeX-like atom classes and spacing, and
  local recovery for unsupported or malformed formulas.
- Fractions, continuous coverage-antialiased radicals with optional indices,
  super/subscripts, display limits, and stretchy delimiters with one-pixel
  strokes for tall round, square, angle, and curly forms. Also included are
  accents, styles, Greek/symbol commands, continued fractions, annotations,
  equation tags in a separate right-side lane, and
  matrix/pmatrix/bmatrix/Bmatrix/vmatrix/Vmatrix, cases,
  array, aligned, and align environments.
- Four math styles, inline baseline/ascent integration, horizontally pannable
  display equations, clipped rule/glyph rendering, and visible local error
  boxes for malformed formulas.
- A 64-entry LRU formula-box cache and deterministic mathematical golden corpus.
- Formula-local, zero-argument `\newcommand`, `\renewcommand`, and `\def`
  aliases with strict count, name, replacement, recursion, and output limits.
- Explicit top-level `&` and `\\` equation breaks using the aligned-layout
  machinery, in addition to aligned and matrix environments.
- Embedded Latin Modern Math—the OpenType successor to LaTeX's traditional
  Computer Modern math face—with glyph variants and layout constants read from
  the same OpenType MATH table. Latin `\text{...}` annotations use that face,
  `\imath`/`\jmath` intentionally use Latin Modern's native dotted
  bold-italic i/j glyphs, and unsupported annotation scripts can still use the
  selected CJK fallback.
- Unicode 17 canonical normalization and full default case folding generated
  reproducibly from the official Unicode Character Database.
- An on-device diagnostics view with load/parse, visible-render and present
  timings, source/IR size, source offset, layout coverage, glyph hit/raster
  counts, and formula-cache activity.
- First-class raw `.md`/`.markdown` and `.txt` opening from a desktop path, the
  in-reader document browser, or an Ndless `argv` file association. Calculator
  wrappers such as `.txt.tns` are recognized directly. Launching without a
  document opens the browser over an empty canvas; no synthetic sample document
  is rendered first.
- Direct `.ttf`, `.otf` (TrueType, CFF, or CFF2), and calculator-wrapped `.tns`
  font loading through a registry-backed Font Manager. Any file can serve one
  or more roles without being loaded twice, and assignments are remembered
  globally across documents and launches.
- A desktop RGB565-to-PPM adapter for repeatable visual inspection.
- Ndless LCD, keypad, key-repeat, touchpad-drag, clock, and file adapters.
- `.md`, `.markdown`, and `.txt` association registration on Ndless.

## Desktop build and test

```sh
make test
build/desktop/nmarkdown-desktop \
  --document samples/phase2.md \
  --events down,page-down,menu,down,enter \
  --no-state \
  --output build/desktop/phase2.ppm
```

The desktop harness replays semantic events, writes the final 320 x 240 image,
and exits. It appends a safe two-step back sequence so an open overlay is closed
before exit and automated runs cannot hang.

## Ndless build

Set `NDLESS_SDK` to an existing SDK and put its tools first on `PATH`:

```sh
export NDLESS_SDK=/path/to/Ndless/ndless-sdk
export PATH="$NDLESS_SDK/bin:$PATH"
make ndless
```

The output is `build/ndless/nmarkdown.tns`. The hardware-safe display path
renders into a stable 320 x 240 RGB565 surface. On HW-W/CX II it rotates that
completed frame into a 240 x 320 staging buffer before synchronization, cleans
the exact blit source's D-cache lines, waits for a fresh PL111
vertical-compare edge, and calls Ndless `lcd_blit`. The synchronized HW-W copy
is therefore contiguous instead of performing a strided rotation while the
LCD is scanning. The syscall retains TI-OS address translation; nMarkdown
never points the LCD controller directly at a heap allocation. The Zehn
declaration advertises the new LCD API, and normal exit restores the OS display
mode.

For external-font heap measurements, build the diagnostic-only
`ndless-memory-profile` target and follow
[docs/MEMORY_PROFILE.md](docs/MEMORY_PROFILE.md). The production package does
not include its allocator wrappers or tracking table.

For deterministic emulator verification, nMarkdown can reuse the pinned,
read-only Firebird integration frontend from a local PocketJS-NSpire checkout:

```sh
export POCKETJS_NSPIRE=/path/to/PocketJS-NSpire
make firebird-test

# Compact fraction/radical/script/matrix framebuffer fixture
make firebird-math-test

# Dotted bold-italic \imath/\jmath and accented-form framebuffer fixture
make firebird-imath-test

# Six-page native catalog/symbol/layout framebuffer gallery
make firebird-symbols-test

# Dense Latin Modern formula and touchpad-navigation fixtures
make firebird-math-review-test
make firebird-oversized-formula-test
make firebird-scroll-swipe-test
make firebird-keymap-test
make firebird-page-test

# Continuous progress endpoints in both touchpad modes
make firebird-progress-test

make firebird-toc-test
make firebird-browser-cancel-test
make firebird-theme-test

# Mixed bold/italic/CJK/monospace/code/math fixture
make firebird-formats-test
```

This builds a test-only `.tns` that creates and directly opens a raw
`.md.tns` fixture on the harness's disposable flash. The harness cold-boots a
CX II image, transfers and launches the native ARM program, records a 320 x 240
PPM capture, and requires stable exact RGB565 samples. Calculator dumps and the
PocketJS-NSpire `config.local.json` remain outside this repository.

For mixed Latin/CJK spacing work, compare the captured Firebird framebuffer
with the matching browser canvas instead of reviewing them separately:

```sh
node tools/firebird/compare-server.mjs \
  --frame=build/firebird/frame.ppm \
  --font=/path/to/the/source-font.ttf
```

Open `http://127.0.0.1:8091/`. The comparison page shows both native 320 × 240
frames and an adjustable blend view using the same source-font outlines and
fixture text.

Calculator controls are summarized below. See the [user guide](docs/USER_GUIDE.md)
and [navigation design](docs/NAVIGATION_DESIGN.md) for context-specific behavior
and acceptance rules. The dated [UI/UX audit](docs/UI_UX_AUDIT_2026-07-14.md)
retains the original evidence; its reproducible P1/P2 interaction findings now
have regression coverage in the current implementation.

- Up/down (or 8/2 outside Search): Up/8 scrolls toward earlier content and
  Down/2 toward later content in both touchpad modes, including
  while wide focus is open.
- Shift or Ctrl + up/down: context-preserving Screen Up/Screen Down in either
  touchpad mode, including while wide focus is open, or several rows in
  supported lists.
- Left/right (or 4/6 outside Search): move one boundary-aligned screen step
  earlier/later; pan a focused wide block; switch menu tabs or settings values
  where applicable. Only Left/Right changes the Search mode.
- Tab or 1: Page Down toward later content. 7: Page Up toward earlier content.
  These directions remain vertical while wide focus is open. Outside Search,
  the aliases also move several rows in lists that support page-sized selection
  movement. In Search, 1 and 7 are query text while Tab remains Page Down.
- Vertical Scroll uses continuous vertical drag and discrete horizontal swipes;
  Horizontal Scroll uses continuous horizontal drag and discrete vertical
  swipes. Natural continuous scrolling uses up/left to advance, while Natural
  discrete swiping follows reading order and uses right/down to advance.
  Separate settings can reverse either behavior. Discrete swipes share the
  same one-line context resolver as Screen Up/Down.
  When wide focus is open, horizontal movement pans the focused canvas instead
  of moving the document.
- In the document, Enter, touchpad center click, or a short touchpad tap can
  activate a link or enter/leave wide-block focus. While any menu or dialog is
  open, a contact tap is deliberately inert: confirm with Enter or a physical
  touchpad click. Modal swipes navigate supported lists and tabs without
  reaching the document underneath; Reader Settings consumes those swipes
  without moving or changing an option. Activation does nothing on ordinary
  document text; theme changes belong in Reader Settings.
- Plus/minus: change text size. Ctrl+Plus/Ctrl+Minus remain TI system
  shortcuts for brighter/dimmer screen backlighting and do not resize text.
- Doc: in Markdown, open the table-of-contents/bookmarks panel when available
  and otherwise show reader controls. In TXT, open a 0–100% position jump.
- Menu: open Reader Settings. Ctrl+T remains an equivalent shortcut.
- Scratchpad: browse and open another Markdown or TXT document. Ctrl+O remains an
  equivalent shortcut.
- Escape: close the visible overlay first; otherwise leave wide-block focus;
  otherwise exit, one context level at a time.
- Ctrl+Escape: exit immediately, including while an overlay is open.
- Ctrl+F: search; letters, all digits, space, and period are available from the
  calculator input adapter, Delete edits, and Enter opens a result. Numeric
  reader-navigation aliases become ordinary digits while Search is open; use
  arrows or Tab to navigate its results.
- Ctrl+N: next search result.
- Ctrl+B: toggle a bookmark at the current source block.
- Ctrl+T: reader settings.
- Ctrl+D: reader diagnostics and benchmark counters.
- Ctrl+O: browse and open another Markdown or TXT document.

The settings panel contains twelve rows, with nine visible at once: Theme
(Light/Dark), Font size (12–22 px, default 15), Line spacing (content-aware Auto
or +2 through +10 px), Side margins (2–18 px, default 5), Tables (Responsive or
Grid + pan), Code blocks (Wrap by default, or Pan), Contrast (Standard or High),
Text sharpness (0–10, default Balanced 5; 0 is extra-smooth and 10 matches the
former Sharpness 7 curve),
Touchpad mode (Vertical Scroll or Horizontal Scroll), Swipe gesture direction
(Natural or Reversed), Scroll gesture direction (Natural or Reversed),
and Fonts. Fonts opens the installed-file manager and its role assignments.
The list scrolls when Fonts is selected. Left/Right updates a row value in the
panel immediately; Enter applies the session and closes Settings (or opens
Fonts from its action row). Document reflow,
palette changes, and the full-screen repaint are committed once Settings
closes; layout changes preserve the current source-relative reading anchor.
Rows other than Fonts are saved per document. Seven external role paths (Body,
Body Italic, Monospace, CJK, Body Bold, Body Bold Italic, and Monospace Italic)
are saved in one checksummed global preference below My Documents and restored directly on the
next launch without rescanning. A missing, moved, or corrupt path safely falls
back safely without preventing the reader from starting.
See the
[user guide](docs/USER_GUIDE.md#54-ctrl-shortcut-routing-while-an-overlay-is-open)
for the complete overlay-switching and nested-picker rules.

## Opening documents directly

Markdown and plain text are opened directly. TI transfer tools store arbitrary
files with a final `.tns` wrapper, so `chapter.md` and `notes.txt` normally
appear as `chapter.md.tns` and `notes.txt.tns`. nMarkdown understands both
forms.

Launch `nmarkdown.tns` once to register the `md`, `markdown`, and `txt`
associations. You can then open a wrapped document from the TI document
browser, launch nMarkdown without an argument to use its own browser, or press
Scratchpad/`Ctrl+O` while reading. Relative Markdown and TXT links also try the
calculator's final `.tns` wrapper automatically.

Markdown remains UTF-8. For `.txt` only, nMarkdown first honors a UTF-8 BOM,
then recognizes ISO-2022-JP escape sequences, strict UTF-8, and finally scores
GBK, Shift-JIS, and EUC-JP candidates. GBK includes GB2312. Common CP932
double-byte extensions are present in the Shift-JIS table; reserved CP932
single bytes `80`, `A0`, and `FD`–`FF` are invalid and recover as U+FFFD.
Legacy mappings are
compact read-only tables in the calculator binary; no `iconv`-style runtime is
required. A byte stream containing only ambiguous legacy Han characters cannot
be identified with certainty, so exact-score ties deterministically prefer
GBK. TXT punctuation such as `#`, `*`, backticks, and `$` stays literal.

## Optional fonts

nMarkdown embeds minimal printable-ASCII DejaVu Sans and DejaVu Sans Mono
faces for its UI, simple ASCII documents, and default code styling. Task checkboxes use fixed 13-pixel controls ported from
GitHub's browser-native checkbox: green with a white tick when checked and a
neutral outline when empty.
Full body, italic, and CJK faces—and a fuller optional monospace face—are external so the calculator
package does not carry several large text families. Latin Modern Math remains
embedded and is always used for formulas.

`make dejavu-font-subsets` prepares optional device-oriented DejaVu Sans,
DejaVu Sans Oblique, and DejaVu Sans Mono files. They are transferable font
assets, not members of `core.fpk`. Copy the desired files into My Documents and
select them through the reader just like any other supported font.

Run `make cjk-font` to produce
`build/fonts/SarasaFixedSC-Regular.ttf.tns`, a 6,105,504-byte Sarasa Fixed SC
Regular asset that fits both the CX 6 MiB and CX II 20 MiB font limits.
Transfer that file into My Documents and assign it to the CJK role (and, when
appropriate, Monospace). It keeps
the GB2312 Simplified-Chinese core, JIS Japanese core, CJK punctuation, kana,
fullwidth forms, and common symbols with uniform CJK cell metrics.

For a different family, copy its font files anywhere under My Documents. There
is no required font directory. TI transfer tools may append `.tns`, so all of
these names are recognized:

```text
reader.ttf
reader.ttf.tns
reader.otf
reader.otf.tns
```

Press Ctrl+T and open **Fonts**. The Font Manager lists individual TTF/OTF files
found under My Documents. Select a file to assign or unassign **Body**, **Body
Italic**, **Body Bold**, **Body Bold Italic**, **Monospace**, **Monospace
Italic**, and **CJK** roles. Its detail page reports detected Latin/CJK
coverage, proportional or fixed pitch, italic/bold style, and variable-font
metadata, and offers **Use suggested roles**. Changes are staged until **Apply
changes** is selected; Esc cancels them.

Roles reference loaded files rather than owning separate copies. Assigning one
Sarasa file to Monospace and CJK therefore creates one FreeType face, one
HarfBuzz font, and one glyph-cache identity. Body uses its assigned role, the
built-in ASCII UI face, and then CJK. Monospace uses its assigned role, the
built-in DejaVu Sans Mono face, and then CJK. Body and Monospace italic companions are
optional and otherwise use FreeType's outline oblique. Latin Modern Math is
fixed and does not appear in the manager.

Font discovery runs only when Fonts is opened and is cached for the current app
session. It recursively examines all of My Documents, subject to responsiveness
bounds of 2,048 non-hidden entries, 128 font files, and 12 directory levels.
The scan reads only small OpenType directory and metadata ranges, not each
font's glyph payload. After **Apply changes**, assigned Stdio-backed files stay
open as random-access streams: FreeType reads outlines on demand and HarfBuzz
requests only the SFNT tables needed for shaping. The complete file is not
copied into a retained heap buffer.
`Font search stopped early` is shown if a bound is reached.

Fonts selected through this menu are read unchanged from the calculator. There
is no web converter and no runtime font-pack format. Copyright, license, URL,
and descriptive OpenType metadata remain part of the original font file.
HarfBuzz uses its OpenType shaping and positioning tables before the minimal
FreeType build rasterizes the selected TrueType, CFF, or CFF2 outlines.

## Embedded font assets

The checked-in embedded `assets/core.fpk` and its C++ blob are reproducible:

```sh
make fontpack
make mathfont
make ui-font-subset
make dejavu-font-subsets
make cjk-font
make unicode-tables
```

The source manifest is `assets/core-font-pack.json`. Exact font versions,
hashes, and the deterministic Sarasa subset command are in
[`assets/fonts/UPSTREAM.md`](assets/fonts/UPSTREAM.md). Font and rasterizer
licenses are recorded in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
The small DejaVu UI and Mono bootstraps keep only the English family, style, full-name,
and PostScript identity records; copyright, license, URL, description, and
legacy/localized `name` records are omitted from that embedded subset. The
project attribution remains in `THIRD_PARTY_NOTICES.md`. Latin Modern Math
retains its complete source metadata because it is embedded without glyph
subsetting. The pack manifest does not append another standalone license
payload and the UI does not display font metadata.

## Layout

```text
include/nmarkdown/    portable public interfaces
src/app/              event loop and viewer state
src/render/           RGB565 surfaces and primitives
src/document/         UTF-8, Markdown IR, entities, and durable reader state
src/layout/           Fenwick height index and virtual block/line layout
src/math/             bounded LaTeX parser, math boxes, cache, and renderer
src/text/             font loading, shaping, cache, and A8 compositing
src/platform/desktop/ desktop snapshot harness
src/platform/ndless/  calculator adapters
tools/fontpack/        build-time embedded-font and math-metadata generators
tools/entities/        reproducible HTML5 named-entity table
tools/unicode/         Unicode 17 folding/normalization table generator
tests/                portable platform and text-stack tests
```

## Compatibility boundaries

The shipped baseline covers left-to-right Latin, Greek, Cyrillic, symbols, and
optional CJK fonts. HarfBuzz OpenType shaping is active for fallback-aware LTR
font/script runs, including substitutions, positioning, kerning, ligatures,
mark attachment, and source clusters. Paragraph-level bidirectional ordering
remains unsupported, so right-to-left and mixed-direction paragraphs are not a
compatibility claim. Footnote and raster-image presentation and a MicroTeX
backend remain optional Phase 5 candidates from `PLAN.md`; they are not
represented as completed features. The diagnostics view records portable
counters, while peak memory and the provisional CX timing targets still require
measurement on each supported calculator model.
