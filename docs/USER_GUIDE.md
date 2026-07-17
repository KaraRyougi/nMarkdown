# nMarkdown User Guide

nMarkdown is a native Markdown and plain-text reader for Ndless-enabled
TI-Nspire CX and TI-Nspire CX II calculators. It opens ordinary source files
directly without a converted document format.

This guide describes the current production implementation. Remaining scope
boundaries and hardware-only checks are collected in section 14.

## 1. Install nMarkdown

1. Install a compatible Ndless release on the calculator.
2. Build nMarkdown with `make ndless` and use
   `build/ndless/nmarkdown.tns`, or obtain a compatible prebuilt
   `nmarkdown.tns`.
3. Transfer `nmarkdown.tns` anywhere under **My Documents**.
4. Launch `nmarkdown.tns` once. On every launch, nMarkdown attempts to register
   itself as the handler for the `md`, `markdown`, and `txt` extensions.

The current build does not report whether file-association registration
succeeded. If opening a Markdown or TXT file from the TI document browser does not
launch nMarkdown, launch `nmarkdown.tns` directly and use its file browser.

## 2. Put document files on the calculator

Transfer the original `.md`, `.markdown`, or `.txt` file without packing or
converting it. TI transfer software normally adds a final `.tns` wrapper to
arbitrary files, so these are all valid document names:

```text
chapter.md
chapter.md.tns
notes.markdown
notes.markdown.tns
book.txt
book.txt.tns
```

nMarkdown recognizes the extensions without regard to letter case. If it is
asked to open `chapter.md` but that exact path is absent, it also tries
`chapter.md.tns`.

There are three ways to open a document:

1. Select its `.md.tns`, `.markdown.tns`, or `.txt.tns` file in the TI document
   browser after nMarkdown has registered the associations.
2. Launch nMarkdown with no document. Its startup browser scans My Documents.
3. While reading, press Scratchpad or `Ctrl+O` to open another document.

When Scratchpad or `Ctrl+O` starts the first recursive scan in a session,
nMarkdown presents an animated `Finding documents` card before filesystem work
begins. The resulting list is cached for later browser opens in that app
session, so returning to the browser does not repeat the scan.

The header shows the document's filename. A final `.tns` is hidden, but the
source extension remains; for example, `chapter.md.tns` is shown as
`chapter.md`, while `book.txt.tns` is shown as `book.txt`.

### Plain-text encodings

Markdown files remain UTF-8. For `.txt` files only, nMarkdown automatically
supports UTF-8 (with or without BOM), GB2312/GBK, Shift-JIS, EUC-JP, and
ISO-2022-JP. GB2312 is a subset of the GBK decoder. JIS X 0208 and the JIS X
0212 extensions used by EUC-JP/ISO-2022-JP-1 are included. Common CP932
double-byte extensions are included in the Shift-JIS mapping, but reserved
CP932 single bytes `80`, `A0`, and `FD`–`FF` are intentionally invalid and
recover as U+FFFD.

Detection is deterministic: a UTF-8 BOM wins, recognizable ISO-2022-JP escape
sequences come next, valid UTF-8 comes next, and legacy Chinese/Japanese
candidates are scored from byte validity and script evidence. A short file
containing only legacy-encoded Han characters can be genuinely ambiguous; an
exact tie prefers GBK. Invalid legacy sequences are replaced locally with
U+FFFD, while binary signatures, NUL/control data, malformed ISO escape streams,
and UTF-8 errors following an explicit BOM are rejected.

TXT is rendered literally. Markdown-looking characters such as `#`, `*`,
backticks, links, and `$...$` formulas are ordinary text. Physical line endings
are retained as line breaks.

### Supported Markdown content

The current parser/layout path supports paragraphs, headings, emphasis, strong
text, strikethrough, links, unordered and ordered lists, task lists,
blockquotes, fenced/inline code, horizontal rules, tables, and inline/display
math spans. Raw HTML is disabled.

Internal `#heading` links jump within the current Markdown document. Relative
`.md`, `.markdown`, and `.txt` links open another document and also try the TI
transfer wrapper automatically. External URLs are shown in a view-only dialog;
nMarkdown does not launch a web browser. Any other relative target produces a
Linked asset notice instead of displaying the asset.

Math supports the reader's bounded LaTeX-like subset, including fractions,
radicals, super/subscripts, common symbols and Greek commands, accents,
stretchy delimiters, matrices, cases, and aligned expressions. In `align` and
`aligned`, `\tag` labels occupy a separate right-side lane and do not change
the equation-column positions. An unsupported or malformed formula is rendered
as a local error box rather than aborting the document. In this reader,
`\imath` and `\jmath` intentionally use the native dotted bold-italic i and j
glyphs from the embedded Latin Modern Math face. Their dots remain present when
the commands appear below an accent.
Latin text inside `\text{...}` and formula tags uses the same math face rather
than the selected document body font.
The [mathematical LaTeX support reference](MATH_SUPPORT.md) gives the exact
514-command native-symbol boundary, supported structures and styles, font
exclusions, and syntax that intentionally remains outside this bounded engine.

## 3. Startup and in-document file browsers

The browser recursively scans My Documents, skips hidden files and folders, and
sorts matching files without regard to case.

When nMarkdown launches without a document, the canvas behind this browser is
empty. The app does not render a synthetic sample page while it waits for a
selection.

- Up/Down or 8/2 moves one file.
- Shift+Up/Down, Ctrl+Up/Down, or the Page Up/Down aliases 7 and Tab/1 move
  five files in the corresponding direction.
- Enter opens the selected file.
- Doc closes the browser.
- Esc cancels the browser.
- Ctrl+Esc exits nMarkdown immediately.

Esc has two different results:

- In the startup browser, no document exists behind the list, so Esc exits to
  TI-OS.
- In a browser opened with Scratchpad or `Ctrl+O`, Esc returns to the current
  document without changing its position, settings, or session fonts.

At startup, Doc, Scratchpad, and `Ctrl+O` cancel the browser just like Esc and
return to TI-OS. In a browser opened over a document, those inputs close the
browser and return to that document.

The browser is a recursive flat list. Ordinary entries show a basename;
duplicates append the shortest distinguishing parent suffix. It lists at most
256 Markdown/TXT files, descends through at most 12 folder levels below its root,
and displays a non-selectable `256-file limit reached` row when capped.
Unreadable nested folders are skipped.

## 4. Reader screen

The reader uses the complete 320 x 240 display:

- A two-pixel progress track spans the top edge in both reading modes. Its
  colored fill is empty at the beginning and reaches the full width only at the
  exact end.
- The filename uses the header width; no synthetic page count competes with it.
- The document reaches both horizontal screen edges; Side margins set the exact
  left and right prose bounds.
- The final two screen rows are kept free of document text.

In both modes, the fill follows the continuous scroll offset relative to the
largest possible offset. A document that fits in one viewport has no traversable
reading distance and therefore remains at 0%.

A document-load error is a status exception: it replaces the reading indicator
with a full-width red alert track; see
[Errors and recovery](#13-errors-and-recovery).

### Loading feedback

Quick document and font operations open directly without flashing an
intermediate screen. If file discovery or reading lasts beyond the short
no-flash delay—or a probed document/font is already known to be large—the
reader presents a compact status card before continuing the remaining
synchronous work. Its text identifies stages such as scanning, reading,
decoding/parsing, preparing the first screen, or rebuilding the font layout.
The card disappears on both success and error; it never becomes a dialog that
requires Enter or Esc.

## 5. Complete calculator controls

### 5.1 Document controls

| Physical input | Current document behavior |
|---|---|
| Up / Down; 8 / 2 | Up/8 scrolls toward earlier content and Down/2 toward later content in both reading modes, including while wide focus is open |
| Shift+Up / Shift+Down | One boundary-aligned screen step toward earlier/later content in either mode, including while wide focus is open |
| Ctrl+Up / Ctrl+Down | Same behavior as the corresponding Shift combination |
| Left / Right; 4 / 6 | One screen step earlier/later; with wide focus, pan left/right by 12 pixels |
| Tab / 1 | One screen step toward later content, including while wide focus is open |
| 7 | One screen step toward earlier content, including while wide focus is open |
| Touchpad vertical movement | Vertical Scroll: continuous drag; Natural Scroll makes up advance. Horizontal Scroll: one discrete screen step; Natural Swipe makes down advance |
| Touchpad horizontal movement | Vertical Scroll: one discrete screen step; Natural Swipe makes right advance. Horizontal Scroll: continuous drag; Natural Scroll makes left advance. With wide focus, pan the focused canvas instead |
| Enter, touchpad center click, or short touchpad tap | Activate the current block's link; choose from a list when it has several; otherwise enter/leave a visible formula or current-block wide focus; otherwise no action |
| Plus / Minus | Increase/decrease body font size by one pixel, clamped to 12–22 px |
| Ctrl+Plus / Ctrl+Minus | Pass through to the TI system shortcut for brighter/dimmer screen backlighting; does not change reader font size |
| Doc | Markdown: open Table of Contents/Bookmarks when available, otherwise Reader Controls. TXT: open a 0–100% position jump |
| Menu | Open Reader Settings; equivalent to Ctrl+T |
| Scratchpad | Open the document browser; equivalent to Ctrl+O |
| Esc | Close the visible overlay; otherwise leave wide focus; otherwise exit |
| Ctrl+Esc | Exit from any state |
| Ctrl+F | Open Search |
| Ctrl+N | Navigate to the next stored search result |
| Ctrl+B | Add/remove a bookmark at the current source block |
| Ctrl+T | Open Reader Settings |
| Ctrl+D | Open Reader Diagnostics |
| Ctrl+O | Open the document browser |

Enter does not provide an inline link cursor. If the current top block contains
several links, it opens a keyboard-navigable chooser for that block. On ordinary
text with no actionable target, Enter/click is deliberately inert and never
changes the theme.

### 5.2 Key repeat and simultaneous input

Up, Down, Left, Right, their numeric aliases 8/2/4/6, Tab, 1, 7, and Delete
repeat after approximately 360 ms, then every 70 ms. Scratchpad, characters,
and Plus/Minus do not repeat. While a keyboard key is held, touchpad events are
not processed.

Touchpad direction compares native X/Y movement at their physical scale. It
does not divide by each full axis range, which would incorrectly stretch the
rectangular touchpad into a square. A contact may jitter by about 1.25% of the
shorter native span without losing tap eligibility. Dragging starts after a
common deadzone of about 4% and only when one axis leads the other by at least
4:3. Before a named swipe is emitted, one provisional axis correction is
allowed if accumulated motion makes the other axis clearly dominant; this
recovers from landing wobble that could otherwise swallow left/right input.
The corrected axis then remains locked until release. After lock, an
approximately 1.25% motion deadzone filters noisy samples. Slow movement stays
responsive because suppressed samples accumulate. A named swipe is emitted
once after 20% travel along the locked axis.

Esc has priority over other keys. Consequently, `Ctrl+Esc` exits even when an
overlay is open. The app still attempts a final state save, but immediate exit
cannot pause to display a save-failure message.

### 5.3 Overlay controls at a glance

| State | Up/Down | Modified Up/Down | Left/Right | Enter | Esc |
|---|---|---|---|---|---|
| TOC / Bookmarks | Move one item | Move five items | Switch tab when bookmarks exist | Jump and close | Close |
| Search | Move one result | Move four results | Change search mode | Jump and close | Close |
| TXT percentage jump | No action | No action | No action | Jump to typed 0–100% value | Close |
| Settings | Move one row | No action | Change value | Apply and close/open Fonts | Close |
| Font Manager | Move one file/action | Move five rows | No action | Open font or apply changes | Cancel and close |
| Font detail | Move one role/action | Move five rows | No action | Toggle role/run action | Return to Font Manager |
| Document browser | Move one file | Move five files | No action | Open file | Cancel; exits if startup browser |
| Diagnostics | No action | No action | No action | No action | Close |
| Message/error dialog | No action | No action | No action | Close; an exit-time state warning also completes exit | Close; an exit-time state warning also completes exit |
| Reader Controls | No action | No action | No action | No action | Close |

The numeric aliases follow these same columns outside Search: 8/2 act as
Up/Down, 4/6 act as Left/Right, and 7/1 act as Page Up/Page Down. Tab remains
Page Down everywhere. While Search is open, every digit is query text; use the
arrow keys or Tab to navigate results and modes. For other list overlays, Page
Up/Down moves by the same multi-row amount as modified Up/Down. Passive or
nested overlays still consume unsupported actions without changing the covered
document.

Touchpad gestures are owned by the visible overlay. In TOC/Bookmarks, Search,
font lists, the document browser, and the link chooser, swipe up advances by
the same multi-row amount as Page Down and swipe down returns by the Page Up
amount. Reader Settings ignores swipes; use Up/Down to select rows and
Left/Right to change the selected option. Keyboard Enter or a physical center
click applies the settings session and closes it; on Fonts, it opens the font
manager. Swipe left/right still mirrors Left/Right for a TOC tab or Search
mode. A short contact tap is ignored by every menu and dialog; it never
selects a font, opens a file, jumps to a section, changes a setting, or
dismisses a panel. Enter or a physical center click activates the selected row
in interactive lists. A physical center click also closes Reader Controls,
Diagnostics, and message dialogs; keyboard Enter remains inert in Reader
Controls and Diagnostics. Continuous drag events are consumed so the covered
document never scrolls as a side effect.

### 5.4 Ctrl-shortcut routing while an overlay is open

Primary overlays can switch directly to one another. Nested choosers consume
unrelated shortcuts so they cannot mutate the covered document.

| Open state | Ctrl+F | Ctrl+T | Ctrl+D | Ctrl+O | Ctrl+N | Ctrl+B | Doc |
|---|---|---|---|---|---|---|---|
| Document | Open Search | Open Settings | Open Diagnostics | Open browser | Next stored result | Toggle bookmark | Open Markdown TOC/Bookmarks/Controls or TXT percentage jump |
| TOC / Bookmarks | Switch to Search | Switch to Settings | Switch to Diagnostics | Open browser | Ignored | Ignored | Close |
| TXT percentage jump | Switch to Search | Switch to Settings | Switch to Diagnostics | Open browser | Ignored | Ignored | Close |
| Settings | Switch to Search | Close | Switch to Diagnostics | Open browser | Ignored | Ignored | Switch to Markdown TOC/Bookmarks/Controls or TXT percentage jump |
| Reader Controls | Switch to Search | Switch to Settings | Switch to Diagnostics | Open browser | Ignored | Ignored | Close |
| Search | Close Search | Switch to Settings | Switch to Diagnostics | Open browser | Move result selection down | Ignored | Switch to Markdown TOC/Bookmarks/Controls or TXT percentage jump |
| Diagnostics | Switch to Search | Switch to Settings | Close | Open browser | Ignored | Ignored | Switch to Markdown TOC/Bookmarks/Controls or TXT percentage jump |
| Font Manager/detail | Ignored | Ignored | Ignored | Ignored | Ignored | Ignored | Ignored |
| Document browser | Ignored | Ignored | Ignored | Close | Ignored | Ignored | Close |
| Message/error dialog | Ignored | Ignored | Ignored | Ignored | Ignored | Ignored | Close |

Plain Plus/Minus is ignored while any overlay is open. Ctrl+Plus/Minus remains
owned by the TI system backlight handler in every reader state. `Ctrl+Esc`
exits from every row in the table.

The physical Menu key sends the same Settings action as `Ctrl+T`: it opens
Settings from the document or another primary overlay and closes Settings when
pressed again. Nested font/file choosers, dialogs, and the document browser
continue to consume both actions rather than replacing their current layer.
Scratchpad sends the same Open Document action as `Ctrl+O`, including the same
overlay-routing and cancellation rules shown in the table.

Overlay text uses a fixed, document-independent hierarchy sized for the
320 x 240 screen: panel titles and ordinary list rows are 13 px, the compact
twelve-row Settings list shows nine 12 px rows at once, auxiliary/status text is
11 px, and the Search query is 14 px. Changing the document's Font size does
not change these menu sizes.

### 5.5 Reader Diagnostics

Press `Ctrl+D` in the document to open a read-only diagnostics panel. It shows
load/parse time, first/last/peak render timing, present timing, source and IR
sizes, current source offset, measured/cached layout counts, glyph-cache
activity, formula-cache activity, and the provisional memory target. The panel
does not scroll and has no selectable rows. Press Ctrl+D or Esc to close it;
Doc switches to the Markdown TOC/Bookmarks panel or Reader Controls, or to the
percentage jump for TXT.

## 6. Touchpad modes and screen steps

### Vertical Scroll

Vertical Scroll is the default. Vertical touchpad drag scrolls continuously;
a horizontal swipe performs one boundary-aligned screen step. With Natural
Scroll, drag up advances. With Natural Swipe, swipe right advances. Either
setting can be reversed independently.

### Horizontal Scroll

Horizontal Scroll swaps the gesture roles. Horizontal touchpad drag scrolls
continuously; a vertical swipe performs one boundary-aligned screen step.
With Natural Scroll, drag left advances. With Natural Swipe, swipe down
advances. Either setting can be reversed independently.

### Boundary-aligned screen steps

Keys never reverse: Up/8 and Down/2 always move by one line; Page Up/7 and
Page Down/1/Tab move one screen step earlier/later; Left/4 and Right/6 do the
same except while a wide block owns horizontal focus.

A forward screen step uses the bottom edge of the old viewport. If a visual
line is clipped there and less than 85% was visible, the new screen starts at
that line's top so it can be read completely. If at least 85% was visible, the
new screen starts at the first following line and does not repeat it. An
immediate reverse returns to the exact previous top rather than estimating a
new boundary. A formula or other row taller than the viewport still guarantees
forward progress.

Both modes show continuous 0–100% progress and no page number. Markdown progress
uses its reflowed document position; TXT uses the current source-byte offset.
Changing the touchpad mode changes only which physical axis is continuous; it
does not snap or otherwise move the document. The exact rationale and
acceptance matrix are recorded in [Navigation design](NAVIGATION_DESIGN.md).

The reading viewport is 220 pixels tall: 240 screen pixels minus the 18-pixel
header and two-pixel bottom inset. Because layout is measured lazily, the total
document height and progress denominator can adjust while previously unseen
content is laid out. Lines in a shaped block that do not intersect the screen are rejected
before glyph rasterization or compositing.

Markdown keeps its complete MD4C document IR and lazily shapes blocks around the
viewport. TXT uses a separate streaming path with no Markdown IR, global line
index, or exact page count. It shows the first screen after reading about
64 KiB, reserves up to a 1 MiB sequential source window, and extends that
window in 32 KiB idle reads. It retains five preceding and ten following shaped
screens. Each bounded run is admitted at a physical-line/whitespace boundary
where possible, shaped once with HarfBuzz, and divided into cluster-safe lines
and screens; fixed 256-byte fragments are no longer shaped independently.
Glyphs for the next line or page are warmed only after the currently requested
frame has been presented. Reverse page movement uses a small ring of recent
page-start byte offsets. Search performs its sequential source scan only while
the Search panel is open.

## 7. Wide code, grid tables, and formulas

Code blocks, Grid + pan tables, and overflowing formulas can use local
horizontal focus. A code block may be focused even when the document's Code
blocks setting is **Wrap**: focus creates a temporary unwrapped canvas for the
original source lines without changing the saved Wrap setting. Formulas retain
their authored rows and native readable size; the reader does not wrap
individual `align` cells and destroy their column relationships.

1. Bring the wide block into view. Code and tables use the current top block;
   Enter or a touchpad click selects a visible overflowing formula even if a
   short prose block remains above it.
2. Press Enter or click the touchpad. An accent outline and edge markers show
   wide focus. A wrapped code block temporarily snaps to the top of its
   unwrapped focus canvas.
3. Press Left or Right to pan by 12 pixels.
4. Use the horizontal touch gesture to pan: in Vertical Scroll, swipe by nearly
   a full viewport; in Horizontal Scroll, drag continuously. Up/Down and Page
   Up/Page Down always remain vertical document navigation while focus is open.
5. Press Enter again or Esc to leave focus and reset pan to the left edge. For
   a temporarily unwrapped code block, this also restores the exact scroll
   position from before focus opened.

Pan is clamped to the current block's measured overflow. Wrapped code enters
focus only when an original unwrapped source line is wider than the viewport;
other blocks that already fit do not enter a false wide-focus state. Formula
glyphs, rules, annotations, and tags move together. The right marker remains
visible while more content exists to the right; after panning, the left marker
shows that hidden content can be recovered.

A formula taller than the 220-pixel reading viewport does not become a navigation
dead end. Line scrolling can traverse it continuously, and Page Down advances
through successive overlapping portions before continuing into the content
after the formula; Page Up retraces those portions toward earlier content.

## 8. Table of contents and bookmarks

### Table of contents

Doc opens the table of contents when the document has headings. Entries are
generated from Markdown heading levels 1–6.

- Up/Down moves one heading.
- Shift+Up/Down or Ctrl+Up/Down moves five headings.
- Enter jumps to the selected heading and closes the panel.
- Doc or Esc closes the panel.
- Right opens the Bookmarks tab when at least one bookmark exists; Left returns
  to the TOC.

The selected TOC heading is retained and is stored with the document state.

### Bookmarks

Press `Ctrl+B` in the document to toggle a bookmark at the source block nearest
the top of the viewport. Up to 256 bookmarks are stored per document and sorted
in source order.

When the current source anchor exactly matches a bookmark, a small accent marker
appears near the lower-left corner. There is no confirmation toast, so this
marker may be the only visible feedback.

Press Doc and then Right to select Bookmarks. Navigate it like the TOC and
press Enter to jump. Press `Ctrl+B` again at the same source block to remove the
bookmark.

Opening another document resets the overlay state. An empty Bookmarks tab shows
an explicit empty state, and Enter cannot activate a retained TOC selection
through it.

### TXT percentage jump

TXT has no stable exact page count because it is streamed and shaped around the
current position. Press Doc to open its percentage jump instead. Type a value
from 0 through 100 and press Enter to seek near that source position. Numeric
navigation aliases, including 1 and 7, are entered as digits while this panel
is open. Delete edits the value; Doc or Esc closes the panel without moving.

## 9. Search

Press `Ctrl+F` to open Search. The initial mode is **ASCII fold**.

### Search text entry

The production calculator keyboard adapter accepts:

- `A`–`Z`; hold Shift for uppercase.
- Digits `0`–`9`. The six digits that act as reader-navigation aliases outside
  Search become ordinary query text while the Search panel is open.
- Space.
- Period.
- Delete to remove the previous complete UTF-8 character.

Arbitrary punctuation and CJK characters cannot currently be typed from the
calculator keyboard. The query limit is approximately 64 UTF-8 bytes, not 64
Unicode characters.

Search examines the original decoded document source, not only the visible
rendered text. Markdown punctuation and link destinations can therefore
participate in matches for Markdown; TXT is already literal source text.

### Search modes

Press Left or Right to cycle through four modes:

1. **Exact** — bytes and case must match exactly.
2. **ASCII fold** — `A`–`Z` compare without case; non-ASCII text remains exact.
3. **Canonical** — canonically equivalent Unicode sequences match, such as a
   precomposed accented letter and the equivalent base-plus-combining sequence.
4. **Unicode fold** — canonical matching plus full Unicode default case folding.

Starting at ASCII fold, Right moves to Canonical, Unicode fold, Exact, then
back to ASCII fold. Left cycles in reverse.

### Results

- Search returns at most 128 results.
- Up/Down moves one result and clamps at the ends.
- Modified Up/Down or Tab moves four results.
- Left/Right changes the search mode.
- Every digit, including `1`, `2`, `4`, `6`, `7`, and `8`, appends to the
  query instead of navigating Search.
- Enter jumps to the selected result, closes Search, and highlights the match.
- `Ctrl+F` or Esc closes Search without clearing its query.
- After a result has been activated, `Ctrl+N` in the document advances through
  stored results and wraps at the end.

The query and selected mode remain available while nMarkdown stays open,
including after switching documents. Results are cleared on a document switch
and recomputed for the new document when Search reopens. Query and mode are not
stored in `.nmdstate` and reset after a complete app restart.

While Search is open, Menu or `Ctrl+T` switches to Settings, `Ctrl+D` switches
to Diagnostics, and Doc switches to Markdown TOC/Reader Controls or the TXT
percentage jump. `Ctrl+O` opens the document browser. Search snippets remove
common Markdown punctuation, include the containing heading, and use word-safe
ellipses.

## 10. Reader settings

Press Menu or `Ctrl+T` to open Settings. Up/Down selects one of twelve rows; nine
are visible at once and the list scrolls to reveal Fonts. Left, Right, or Enter
changes the selected value; for two-state options all three inputs toggle
rather than having directional semantics. The row label updates
immediately, but the covered document keeps its existing layout and paint while
the panel remains open. Esc, Menu, another `Ctrl+T`, or switching to another
primary overlay closes Settings and commits all pending document reflow,
palette changes, and one full-screen repaint. Closing does not undo the selected
values.

The selected settings row is retained when the panel is reopened.

### Theme

- Values: **Light**, **Dark**.
- Default: **Light**.
- Effect: changes the document, header, overlay, selection, code, and accent
  palette without reflowing text.
- Persistence: per document.

Light mode applies a fixed-point contrast curve to grayscale glyph and
math-rule edges before RGB565 quantization. It suppresses pale outline fringe
and reinforces high-coverage stem pixels for a sharper edge. Dark mode uses a
separate mid-coverage lift so small light-on-black text remains clear. Both
curves preserve exact transparent/opaque endpoints.

Theme changes are available only from this Settings row. Enter or a touchpad
click on ordinary document text does not change the theme.

### Font size

- Values: every integer from **12 px** through **22 px**.
- Default: **15 px**.
- Effect: reflows prose, headings, code, and inline content around the current
  source anchor.
- Persistence: per document.
- Shortcut: Plus/Minus changes the same value outside isolated overlays.

### Line spacing

- Values: **Auto**, then **+2 px** through **+10 px** in one-pixel steps.
- Default: **Auto**.
- Effect: Auto uses about one fifth of the font em as normal leading, bounded to
  2–5 pixels, and expands further for tall glyphs or formulas. Manual values set
  a nominal body-size-plus-gap line height, but tall content can still require
  additional clearance.
- Persistence: per document.

There is no `+1 px` value. Left from `+2 px` returns to Auto.

### Side margins

- Values: every integer from **2 px** through **18 px**.
- Default: **5 px** on both sides.
- Effect: changes prose line width to exactly `320 - 2 × margin`, then reflows
  around the current source anchor. Code blocks and tables use these same outer
  bounds with their own equal left/right padding inside the box.
- Persistence: per document.

### Tables

- Values: **Responsive**, **Grid + pan**.
- Default: **Responsive**.
- Responsive effect: omits the header row as a standalone row and renders each
  data row as wrapped header/value records.
- Grid + pan effect: preserves measured columns without wrapping and permits
  local wide-block focus and horizontal pan.
- Persistence: per document.

### Code blocks

- Values: **Pan**, **Wrap**.
- Default: **Wrap**.
- Pan effect: preserves source line lengths and uses local wide focus.
- Wrap effect: wraps code to the available content width.
- Persistence: per document.

Wrap remains the document layout default, but Enter/click can still focus a
visible wrapped code block whose original source line exceeds the available
width. That focus view is a temporary unwrapped pan canvas; leaving it restores
the previous scroll position and does not change this setting.

### Contrast

- Values: **Standard**, **High**.
- Default: **Standard**.
- Effect: selects a higher-contrast Light or Dark palette without reflow.
- Persistence: per document.

The visible `Contrast` label updates immediately in the panel; the document
palette changes in the full repaint when Settings closes.

### Text sharpness

- Values: **0** through **10**.
- Default: **5 (Balanced)**.
- **0** is extra-smooth, with lower edge contrast than the former minimum.
- **10** exactly matches the former Sharpness 7 coverage curve; the old
  near-binary 8–10 tail is no longer exposed.
- All levels retain grayscale antialiasing. Intermediate values interpolate
  the Light and Dark theme-specific curves without changing glyph metrics.
- Persistence: per document.

Changing this row does not reload fonts or reflow text. The new coverage curve
is applied in the single full-screen repaint after Settings closes.

### Touchpad mode

- Values: **Vertical Scroll**, **Horizontal Scroll**.
- Default: **Vertical Scroll**.
- Effect: selects whether vertical or horizontal touchpad movement is the
  continuous scroll axis. The other axis performs screen steps.
- Persistence: per document.

Changing this setting does not snap or move the current reading position.

### Swipe gesture direction

- Values: **Natural**, **Reversed**.
- Default: **Natural**.
- Natural effect: follows reading order. A rightward horizontal swipe or
  downward vertical swipe advances.
- Reversed effect: a leftward horizontal swipe or upward vertical swipe
  advances.
- Scope: the discrete screen-step axis only. It does not change continuous
  drag, keys, taps/clicks, modal navigation, or direct manipulation inside
  wide-content focus.
- Persistence: per document.

### Scroll gesture direction

- Values: **Natural**, **Reversed**.
- Default: **Natural**.
- Natural effect: like macOS natural scrolling, content follows the finger.
  An upward or leftward continuous drag advances.
- Reversed effect: a downward or rightward continuous drag advances.
- Scope: the continuous scroll axis only. It does not change discrete swipes,
  keys, taps/clicks, modal navigation, or direct manipulation inside
  wide-content focus.
- Persistence: per document.

### Fonts

- Defaults: minimal printable-ASCII **DejaVu Sans UI** and **DejaVu Sans Mono** bootstraps, native task
  checkbox controls, no external role assignments, and embedded **Latin Modern
  Math** for formulas.
- Values: opens the Font Manager. Each discovered font file can be assigned to
  one or more text roles.
- Persistence: seven role paths are saved in one global
  checksummed preference and restored directly on later launches; they are not
  part of a document sidecar.

See [Optional fonts](#11-optional-fonts) for the complete workflow.

### Settings input ownership

Settings owns navigation while visible. It consumes swipes without moving the
selected row or adjusting its value. Keyboard navigation, Enter, and a physical
touchpad click remain available; a short contact tap is ignored. Page
movement, continuous drag, Plus/Minus, bookmark changes, and next-result
navigation cannot affect the document behind it. Primary overlay shortcuts can
switch panels as shown in section 5.4.

## 11. Optional fonts

nMarkdown includes a 25,644-byte DejaVu Sans UI/body bootstrap and a
17,492-byte DejaVu Sans Mono code bootstrap, each covering printable ASCII and
U+FFFD. Read-only task checkboxes use fixed-size rasters ported from
GitHub's browser-native checkbox: green with a white tick when checked, or a
neutral rounded outline when empty. They add no font payload. The bootstraps are
sufficient for
menus and very simple
ASCII documents, but it does not replace a full reading font. Text fonts are
ordinary TTF/OTF files stored on the calculator. Latin Modern Math is embedded,
fixed for formulas, and is not assignable in the Font Manager.

Run `make cjk-font` and transfer
`build/fonts/SarasaFixedSC-Regular.ttf.tns` into My Documents. This is a
6,105,504-byte Sarasa Fixed SC Regular subset that fits both the original CX
6 MiB limit and the CX II 20 MiB limit. It retains the GB2312
Simplified-Chinese core, JIS Japanese core, CJK punctuation and radicals, kana,
fullwidth forms, and common symbols. Its internal family/style remain
**Sarasa Fixed SC Regular**. The full 24.9 MB upstream font is intentionally
not transferable because it exceeds both device limits.

Copy the original font anywhere under My Documents. Accepted names are:

```text
reader.ttf
reader.ttf.tns
reader.otf
reader.otf.tns
```

There is no web converter. nMarkdown reads the transferred TrueType or OpenType
file unchanged, including its copyright, license, URL, and descriptive
OpenType metadata. Project-supplied CX subsets are optional build artifacts,
not a device file format or a required import step.

### Select a font

1. Press `Ctrl+T`.
2. Move to **Fonts** and press Enter, Left, or Right.
3. The first opening scans all of My Documents for supported files and shows
   **Installed fonts**. Select a file with Enter or a physical touchpad click.
   A short contact tap does nothing. Swipe or Page Up/Down moves five rows.
4. The detail screen shows detected Latin/CJK coverage, proportional or fixed
   pitch, italic/bold style, and whether variable axes exist.
5. Toggle any applicable role, or choose **Use suggested roles**. Assignments
   remain staged; the document is not reflowed yet.
6. Press Esc to return to Installed fonts, then choose **Apply changes**. The
   document reflows once and retains its source-relative reading anchor.

Esc from the Installed fonts list cancels every staged change and returns
directly to the document. To stop using a file, open it, choose **Unload from
roles**, return to the list, and apply. This unassigns and unloads the resource;
it does not delete the font file from My Documents.

### Files and roles

The Font Manager separates resources from their uses. The assignable roles are:

- **Body**, plus optional **Body Italic**, **Body Bold**, and **Body Bold
  Italic** companions.
- **Monospace**, plus optional **Monospace Italic**.
- **CJK**, used as a script fallback.

Missing italic companions use FreeType's outline oblique on their regular role;
missing bold companions use synthetic bold. One file may serve several roles.
For example, assigning Sarasa Fixed SC to Monospace and CJK stores one streamed
resource, creates one `FT_Face` and HarfBuzz font, and shares one glyph-cache
identity.
The Settings summary therefore reports unique loaded files rather than the
number of checked roles.

Automatic suggestions never prevent manual overrides. A proportional Latin
regular face suggests Body; a fixed Latin regular face suggests Monospace; a
fixed Latin+CJK face suggests Monospace and CJK; a proportional Latin+CJK face
suggests Body and CJK. Italic and bold metadata suggest the matching companion
role. Detection uses OpenType style bits, `OS/2.panose`, `post.isFixedPitch`,
Unicode coverage, and the presence of variable-font axes.

Body fallback is `Body → built-in ASCII UI → CJK → replacement`. Monospace
fallback is `Monospace → built-in DejaVu Sans Mono → CJK → replacement`.
Formula symbols and Latin `\text{...}` annotations
use embedded Latin Modern Math. Unsupported annotation scripts may use CJK and
then Body/UI fallback.

Font assignments remain active when another document opens and across
launches. Seven paths—Body, Body Italic, Monospace, CJK, Body Bold, Body Bold
Italic, and Monospace Italic—are stored in the hidden, checksummed `NMF3`
`.nmarkdown-fonts` preference below My Documents. Multiple roles may contain
the same path. Startup deduplicates those paths and loads each file once without
scanning My Documents. Legacy `NMF1`, `NMF2`, and older
`.nmarkdown-cjk-font` preferences are migrated safely.

Font discovery occurs only on the first Fonts opening in an app session. It is
cached for later openings and deliberately bounded because it runs
synchronously on the calculator: at most 2,048 non-hidden filesystem entries,
128 font files, and 12 directory levels. Files in each directory are examined
before its subdirectories, so a font copied directly into My Documents is
found before files in a large nested tree. The scanner reads only small SFNT
table-directory and metadata ranges; it does not load glyph data for every
font. If a discovery budget is reached, the list ends with the visible,
non-selectable row `Font search stopped early`.

Each discovered file has one row, even when several files share an internal
family name. The picker shapes only its visible rows during a frame. Once an
assignment is applied, the calculator keeps a random-access file handle and
reads glyph outlines or shaping tables on demand; it does not retain the
complete file in heap memory.

Per-file limits are 6 MiB on CX and 20 MiB on CX II. The total selected
external-font limit is 8 MiB on CX and 20 MiB on CX II, so the documented
Sarasa subset can coexist with compact DejaVu Body and Monospace assets on the
original CX. A file assigned to several roles counts once toward this limit.
These are on-disk admission limits, not retained-heap budgets. If FreeType
rejects a face, a file is oversized, the aggregate unique-resource limit is
exceeded, an allocation fails, or reflow fails, nMarkdown restores the
previous registry and opens a `Could not apply fonts` dialog with the original
reason.

## 12. Per-document state

When persistence is enabled, nMarkdown uses a checksummed sidecar next to the
opened document. The name is the complete opened path plus `.nmdstate`:

```text
chapter.md.tns
chapter.md.tns.nmdstate
```

The sidecar stores:

- Source-relative reading position.
- Bookmarks.
- Last selected TOC heading.
- Theme and contrast.
- Font size.
- Auto/manual line spacing.
- Side margins.
- Responsive/Grid table mode.
- Pan/Wrap code mode.
- Vertical/Horizontal Scroll touchpad mode plus independent swipe and
  continuous-scroll gesture directions.
- Text sharpness from 0 through 10.

It does not store Reading/Code/CJK family files, search query, search mode,
search results, or the active overlay. Resolved family paths live in the
separate global preference described in section 11.

State is accepted only when its embedded identity matches the current document
contents. Editing the source file changes that identity and causes its old
position/settings state to be ignored.

nMarkdown attempts to save state after a setting or bookmark changes, before a
successfully loaded document replaces the current one, and when the reader loop
exits. It first writes a complete temporary file and uses rename-based atomic
replacement when the filesystem implements it. Some Ndless/newlib combinations
report `ENOSYS`, `ENOTSUP`, or `EOPNOTSUPP` for rename. On those systems,
nMarkdown rewrites the destination from the staged payload, reopens it, and
compares every byte before deleting the temporary copy. That compatibility
fallback is verified but cannot be atomic when rename itself is unavailable.

A corrupt, stale, or unreadable sidecar produces a `Saved state warning`; a
write failure produces `State not saved` once per app session. If a normal Esc
exit encounters that warning, the warning is the second screen in the same exit
flow: press Esc or Enter once to dismiss it and complete the pending exit.
`Ctrl+Esc` remains an immediate exit and therefore cannot pause to show a
save-failure dialog, although the save is still attempted.

If a new document has no valid sidecar, it currently inherits the live settings
from the previously open document in that app session. This is current behavior,
not a clean per-document-default design. Starting a fresh app session uses the
document defaults listed in this guide.

To reset a disposable document during testing, exit nMarkdown and remove only
that document's `.nmdstate` sidecar. Do not remove other document or TI system
files.

## 13. Errors and recovery

### Dialog errors and notices

nMarkdown uses message dialogs for errors encountered while a valid document is
already open, including:

- `Could not open document`.
- `Could not list documents`.
- `Could not list fonts`.
- `Could not load font`.
- `Could not open link`.
- `Link target not found`.
- `Linked asset`.
- `External link`.

Enter, Esc, or Doc closes a message dialog. Other overlay shortcuts are
consumed while the dialog is open.

External URLs are view-only: the dialog shows the target but nMarkdown does not
launch a browser. Relative Markdown links can open another document and then
jump to a heading fragment. Links to non-Markdown assets show a `Linked asset`
notice.

### Document and font limits

- Documents: 4 MiB on CX, 8 MiB on CX II. Decoded TXT must also fit that cap.
- Fonts: 6 MiB per file / 8 MiB total on CX; 20 MiB per file and total on CX II.
- Browser: 256 Markdown/TXT paths and 128 font paths; font discovery additionally
  stops after 2,048 filesystem entries.
- Search: 128 results.
- Bookmarks: 256 per document.

Oversized files report `file exceeds the configured size limit` when a message
dialog is available.

Invalid UTF-8 in Markdown is sanitized with replacement characters rather than
aborting the whole document. Raw HTML blocks/spans are disabled. Raster images
are not displayed; their alt text can appear as ordinary text. Footnote
presentation is not implemented.

### Startup and direct-open errors

If a directly launched document is missing, oversized, unreadable, binary-like,
or cannot be parsed, nMarkdown shows the requested filename, the original
failure reason, and recovery guidance. It does not replace the failed document
with demonstration text. An empty text file opens an explicit empty-document
state.

Recovery options are:

1. Press Scratchpad or `Ctrl+O` and open a valid Markdown or TXT document.
2. Press `Ctrl+Esc` to exit, correct the file, and relaunch.

If listing My Documents itself fails at startup, a `Could not list documents`
dialog appears. Close the dialog, then exit or retry with
Scratchpad/`Ctrl+O` after the filesystem problem is corrected.

## 14. Remaining scope boundaries

The audit's reproducible P1/P2 interaction defects have regression coverage in
the current build. The remaining boundaries are:

1. **The document and font browsers are recursive but flat.** Duplicate
   basenames have distinguishing parent suffixes, but there is no folder tree
   or breadcrumb navigation.
2. **Link selection is block-based.** A block with several links gets a chooser,
   but there is no inline cursor moving between rendered link glyphs.
3. **Calculator text entry is deliberately small.** Search accepts the mapped
   letters, all digits, space, and period. The six numeric reader-navigation
   aliases become query digits while Search is open; arbitrary CJK and most
   punctuation still require a future input method.
4. **Search reads source text.** Markdown result labels clean common syntax and
   add heading context, but they are not a complete semantic index of rendered
   text. TXT does not keep a search index; opening or editing Search starts a
   streaming scan of the file.
5. **A document without a valid sidecar inherits the live settings of the
   previous document** during the same app session. A fresh launch starts with
   the defaults in this guide, except that the last valid global font-family
   selections are restored.
6. **Right-to-left paragraph ordering is not implemented.** HarfBuzz shapes
   fallback-aware left-to-right runs, but full paragraph bidirectional layout
   remains outside the current compatibility target.
7. **Physical hardware verification remains separate.** Firebird validates ARM
   execution and framebuffer behavior, but simultaneous-key timing, touchpad
   routing, filesystem permissions, performance, and memory headroom must also
   be checked on each supported calculator model.

## 15. Quick reference workflows

### Change Dark mode, verify it, then return to Light

1. Press `Ctrl+T`.
2. Select **Theme** and choose **Dark**.
3. Press Esc.
4. Scroll and read in Dark mode.
5. Press `Ctrl+T`; verify **Theme: Dark**.
6. Choose **Light**.
7. Press Esc and continue scrolling.

### Open a custom CJK font

1. Transfer the original `.ttf`, `.otf`, or wrapped `.tns` font under My
   Documents.
2. Press `Ctrl+T`, select **Fonts**, and open the font.
3. Check **CJK**, return to Installed fonts, and choose **Apply changes**.
4. Inspect mixed Latin, Chinese, and Japanese text.
5. To clear it, reopen that file, uncheck **CJK**, and apply.

### Search and continue through matches

1. Press `Ctrl+F`.
2. Type the query.
3. Use Left/Right to choose a mode.
4. Use Up/Down to select a result and press Enter.
5. Press `Ctrl+N` in the document for the next result.

### Exit safely

- With no focus or overlay, press Esc once.
- If an overlay is open, press Esc to close it, then Esc again to exit.
- From a nested font-choice list, Esc returns to families, another Esc closes Fonts,
  and another Esc exits.
- If an exit-time `State not saved` warning appears, press Esc or Enter once;
  dismissing it also completes the pending exit.
- Press `Ctrl+Esc` at any time for a one-step exit with the normal final
  state-save attempt.
