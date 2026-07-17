# Full UI/UX Audit and User Documentation Plan

Revision: 2026-07-15 — navigation-redesign addendum added to the original
option-by-option audit plan.

## Navigation redesign addendum (supersedes legacy SET-08/SET-09)

The original 2026-07-14 cases below preserve the audit history for the former
Scroll/Page Swipe implementation. They are not the acceptance criteria for the
current reader. The following executable cases supersede every legacy
expectation involving `Reading mode`, `Scroll behavior`, page labels, discrete
page progress, direction-fixed swipes, or mode-switch snapping. The normative
model is [Navigation design](NAVIGATION_DESIGN.md).

### NAV-01 — Vertical Scroll with Natural gestures

1. Open a long document at its exact top and record the filename, source anchor,
   and zero-pixel progress fill.
2. Open Settings, press Up ten times, then Down seven times. Verify row 7 reads
   `Touchpad mode: Vertical scroll`.
3. Press Down once. Verify row 8 reads `Gesture direction: Natural`.
4. Close Settings with Esc. Verify the document returns at the exact same top,
   the header has no page number, and progress remains 0%.
5. Drag upward continuously. Verify content advances smoothly and progress
   becomes partial. Start a new contact and drag downward by the same amount;
   verify movement returns toward the start.
6. Return to the exact top. Swipe left once; verify one context-preserving
   forward screen step. Swipe right once; verify the exact original top returns.
7. Reopen Settings. Verify row 8 remains selected and both labels are unchanged.
   Close, scroll one line Down, and verify the reader remains responsive.

### NAV-02 — Horizontal Scroll with Natural gestures

1. Begin at a recognizable nonzero source anchor in Vertical Scroll.
2. Open Settings, normalize to row 7, and press Right. Verify the label becomes
   `Touchpad mode: Horizontal scroll` while the menu remains open.
3. Close with Esc. Verify the exact source anchor is preserved—mode switching
   must not snap to a line, page, or percentage boundary.
4. Drag left continuously. Verify content advances smoothly; drag right in a new
   contact and verify it returns toward the earlier position.
5. Return to the recorded anchor. Swipe up once; verify one forward screen step.
   Swipe down once; verify the exact recorded top returns.
6. Verify the progress fill is still `floor(320 × scroll_y / max_scroll_y)` and
   that no current/total page label is added.
7. Reopen Settings; verify row 7 and Horizontal Scroll. Press Left to restore
   Vertical Scroll, close, perform one line scroll, and verify liveness.

### NAV-03 — Reversed gestures on both axes

Vertical Scroll cycle:

1. Open Settings, normalize to row 8, and verify `Gesture direction: Natural`.
2. Press Right; verify `Gesture direction: Reversed`. Close with Esc.
3. Drag down; verify later content advances. Drag up in a new contact; verify it
   moves earlier.
4. Return to a recorded top. Swipe right; verify one forward screen step. Swipe
   left; verify the exact top returns.
5. Reopen Settings, verify Reversed, press Left to restore Natural, close, drag
   up, and verify the Natural direction works again.

Horizontal Scroll cycle:

6. Open Settings, set row 7 to Horizontal Scroll and row 8 to Reversed, then
   close with Esc.
7. Drag right; verify later content advances. Drag left in a new contact; verify
   it moves earlier.
8. Return to a recorded top. Swipe down; verify one forward screen step. Swipe
   up; verify the exact top returns.
9. Reopen Settings, restore Natural, close, drag left, and verify the Natural
   direction works again. Reopen once more and verify the restored labels.

### NAV-04 — Keys never reverse

Repeat this case under Vertical/Natural, Vertical/Reversed, Horizontal/Natural,
and Horizontal/Reversed:

1. At the top, press Up and 8; verify both are inert at the earlier boundary.
2. Press Down then Up; repeat with 2 then 8. Verify the same one-line
   later/earlier mapping in every configuration.
3. Press Page Down then Page Up; repeat with Tab then 7 and 1 then 7. Verify each
   forward action advances and each immediate reverse returns to the exact top.
4. Press Left at the top; verify it is inert. Press Right; verify a forward
   screen step. Press Left; verify exact reversal.
5. Enter wide focus and verify Left/Right pan the focused canvas conventionally;
   gesture direction must not reverse direct manipulation.
6. Open Search and type 1/2/4/6/7/8. Verify all are query characters rather than
   navigation aliases.

### NAV-05 — Page-boundary alignment, progress, and persistence

1. Use a prose/code fixture with at least twelve complete visual lines. Press
   Page Down and capture before/after frames. Verify a fully displayed bottom
   line is absent from the new frame. Repeat with a deliberately clipped bottom
   line and verify only that incomplete line is retained from its top.
2. Press Page Up immediately and verify the framebuffer returns to the exact
   prior top. Repeat with the mode's discrete swipe axis.
3. Use a formula taller than 220 pixels. Repeatedly press Page Down; verify every
   action moves forward, no step traps inside the formula, and following prose
   is reachable.
4. Continue to the exact end. Verify progress reaches all 320 pixels only there;
   reverse once and verify it becomes partial. Return to the exact start and
   verify it is zero pixels.
5. Switch modes at a nonzero position and verify progress width and source anchor
   do not change. Repeat with a one-viewport document and verify 0% in both modes.
6. Save Horizontal Scroll plus Reversed at a recognizable position, exit, and
   reopen. Verify both settings and the source-relative position restore. Repeat
   with Vertical Scroll plus Natural.
7. Run the same flows in Firebird. Require native input markers, a fresh render
   and presentation after each gesture, continuous progress agreement between
   trace and RGB565 pixels, and a clean Ctrl+Esc exit.

## 1. Objective and Constraints

Conduct a complete UI/UX review of nMarkdown's calculator interface, covering every input state, overlay, navigation path, setting, reading mode, and failure state.

This pass will:

- Produce an evidence-backed UX audit.
- Produce a complete end-user guide containing every hotkey, gesture, menu, and setting.
- Verify behavior through the desktop semantic-event harness and PocketJS/Firebird.
- Document defects and recommended behavior without changing application or test-harness code.
- Preserve font metadata and licensing arrangements. Audit fixtures and optional
  web subsets may retain copyright, license, URL, and descriptive name records.

Current baseline: all 16 desktop tests pass. Passing tests will not be treated as proof of consistent UX because many interaction combinations are not currently covered.

## 2. Repository Deliverables

### Stable user guide

Create `docs/USER_GUIDE.md` as the user-facing source of truth.

It will be written for TI-Nspire CX/CX-II users, not project contributors, and will include:

1. Installing and launching nMarkdown.
2. Opening `.md`, `.markdown`, `.md.tns`, and `.markdown.tns` files.
3. Registering file associations.
4. Understanding the header, progress indicator, document area, overlays, and page counter.
5. Complete control reference.
6. Reading modes and navigation.
7. Table of contents and bookmarks.
8. Search and search modes.
9. Links, wide blocks, code, tables, and formulas.
10. Every reader setting.
11. Managing on-calculator font files and assigning them to one or more roles.
12. Persistence and per-document state.
13. Error messages and troubleshooting.
14. Current limitations.

### Dated audit report

Create `docs/UI_UX_AUDIT_2026-07-14.md`.

The report will contain:

- Audit scope and method.
- Current strengths.
- Canonical interaction expectations.
- Prioritized findings.
- Accessibility and low-resolution usability risks.
- Screenshots tied to individual findings.
- Reproduction steps and expected/current behavior.
- Recommended fixes, even though fixes are outside this pass.
- Evidence limits, especially simulator versus real-hardware gaps.
- A proposed follow-up verification matrix for future fixes.

### Evidence

Create `docs/images/ui-ux-audit-2026-07-14/` containing selected PNG screenshots at the native 320×240 resolution.

Temporary PPM captures, logs, and experimental output will remain under `build/` and will not be treated as durable documentation.

### README integration

Update `README.md` to:

- Link prominently to the user guide.
- Link to the dated audit from the development/testing section.
- Replace the abbreviated control list with a short summary pointing to the authoritative guide.
- Remove or correct statements contradicted by the production input mappings or current implementation.

No public C++ interfaces, state schemas, application behavior, or test code will change.

## 3. Interaction Model to Audit

The audit will explicitly model these UI states rather than treating the application as a single reader screen:

1. Document view in Scroll mode.
2. Document view in Page Swipe mode.
3. Wide code/table/formula focus.
4. Generic reader-controls overlay.
5. Table-of-contents overlay.
6. Bookmark tab.
7. Search overlay.
8. Reader settings.
9. Installed-font list.
10. Font detail and role assignment.
11. Startup document browser.
12. In-document document browser.
13. Diagnostics.
14. Informational, link, and error dialogs.
15. Loading or error screen with no valid document.
16. Return to TI-OS.

Nested states will be distinguished. For example, a font detail screen is a
child of the Installed fonts list, while the startup document browser
has different exit behavior from a browser opened over an existing document.

## 4. Canonical Interaction Contract

Current behavior will be judged against a consistent contract. This section defines the audit's expected behavior, not a claim about shipping behavior. The user guide must document what the production build actually does, including known limitations and inconsistencies. When the current implementation differs from this contract, the audit will show both columns explicitly and will not rewrite the current behavior as though the recommendation were already implemented.

### Escape and exit

- `Ctrl+Esc` exits immediately from every state.
- `Esc` performs one back operation.
- In wide-block focus, `Esc` leaves focus without exiting.
- In a nested font-family choice list, `Esc` returns to family-slot selection.
- In a top-level overlay over a document, `Esc` dismisses the overlay and returns to that document.
- In an in-document file browser, `Esc` cancels selection and returns to the document.
- In the startup file browser, where no document exists behind it, `Esc` exits to TI-OS.
- Only `Esc` from an unobscured document view exits normally.
- Settings apply immediately; dismissing settings does not roll changes back.

### Modal input isolation

While an overlay is open:

- Navigation input must affect the overlay, not the document behind it.
- Touchpad drag or swipe must not scroll or change pages behind the overlay.
- `Enter` must not toggle the theme or focus document content through an overlay.
- Bookmark, search-next, font-size, and other global shortcuts must be evaluated for whether they should remain global or be ignored.
- Switching to another overlay with a shortcut must close the previous overlay cleanly and leave exactly one active UI state.

### Navigation

- Selection must remain visible when lists exceed six rows.
- Up/down and page-up/page-down must stop predictably at boundaries.
- Reopening a menu must produce a useful, stable selection position.
- Repeated TOC and bookmark jumps must remain responsive.
- Every gesture must have a keyboard alternative.
- Selection, focus, active tab, current search mode, and changed settings must be visible.

## 5. Complete Control Inventory for the Guide

The guide will document production TI-Nspire mappings only. Any fixture-only
key substitutions used because PocketJS cannot inject a physical key must be
identified as harness limitations rather than presented as real controls.

### Global controls

| Input | Document behavior | Overlay-dependent behavior |
|---|---|---|
| Up / Down; 8 / 2 | Scroll one line in Scroll mode; move one page in Page Swipe mode | Move list or setting selection |
| Shift+Up / Shift+Down | Scroll approximately one page | Move several rows in supported lists |
| Ctrl+Up / Ctrl+Down | Same page-scroll action | Move several rows in supported lists |
| Left / Right; 4 / 6 | Change page in Page Swipe mode or pan a focused wide block | Change settings, search mode, or TOC/bookmark tab |
| Tab / 1 | Same as Page Down | Move several rows where Page Down is supported |
| 7 | Same as Page Up | Move several rows where Page Up is supported |
| Enter or touchpad click | Activate a link or focused item; enter/leave wide-block focus; otherwise do nothing | Activate selected item or setting |
| Plus / Minus | Increase or decrease body font size | Current actual behavior will be documented and audited |
| Doc | Open TOC/bookmarks when available; otherwise show reader controls | Close or switch the current section panel according to actual behavior |
| Menu | Open Reader Settings | Switch to or close Reader Settings according to actual behavior |
| Scratchpad | Open the Markdown browser | Same routing as Ctrl+O; nested overlays may consume it |
| Esc | Leave focus, dismiss one overlay level, or exit | Context-sensitive back/cancel |
| Ctrl+Esc | Exit immediately | Exit immediately |
| Ctrl+F | Open search | Close/reopen or switch to search, depending on current state |
| Ctrl+N | Navigate to the next stored search result | Move the result selection while search is open |
| Ctrl+B | Toggle a bookmark at the current source block | Audit whether this can mutate the document behind overlays |
| Ctrl+T | Open or close reader settings | Switch from another overlay or close settings |
| Ctrl+D | Open or close diagnostics | Dismiss diagnostics |
| Ctrl+O | Open the Markdown browser | Replace the current overlay with the document browser |

### Search input

Document:

- Letters `A-Z`.
- Shift for uppercase letters.
- Digits `0`–`9`; 1/2/4/6/7/8 become text while Search is open even though
  they remain navigation aliases elsewhere.
- Space.
- Period.
- Delete to remove the previous UTF-8 character.
- Up/down to select results.
- Modified up/down to move several results.
- Left/right to switch search modes.
- Enter to navigate to the selected result.
- `Ctrl+N` for subsequent results.
- `Ctrl+F` or `Esc` to close search.

The four modes will be explained in plain language:

1. Exact UTF-8.
2. ASCII case-insensitive.
3. Canonical Unicode equivalence.
4. Unicode case-insensitive.

The guide will explicitly state that the physical calculator input adapter currently cannot type arbitrary punctuation or CJK search queries.

### Touchpad

Document:

- Vertical drag performs continuous movement in Scroll mode, using the saved
  Natural/Traditional Scroll behavior.
- Vertical swipes change pages in Page Swipe mode.
- Horizontal drag performs continuous movement in Page Swipe mode, using the
  same Scroll behavior.
- Horizontal swipe performs Page Up/Page Down.
- A physical touchpad click is equivalent to Enter, including in Reader
  Settings, where it applies the session instead of changing the selected
  value. A short contact tap is ignored while any menu or dialog is open,
  preventing accidental activation while positioning a finger.

Direction classification compares native X/Y deltas at their physical scale;
normalizing by each full range would distort the rectangular pad. Tap slop,
drag-start, and continuous-motion deadzones are separate; the chosen axis must
lead by 4:3. One correction before a named swipe recovers from landing wobble,
then the axis remains locked until release. Sub-deadzone movement accumulates.

The audit will verify swipe direction, threshold behavior, accidental diagonal activation, and input isolation while overlays are open.

## 6. Settings Documentation

The guide will include defaults, ranges, effects, and persistence for all twelve rows.

| Setting | Default | Values | Effect |
|---|---:|---|---|
| Theme | Light | Light, Dark | Changes document and overlay palette |
| Font size | 15 px | 12-22 px | Reflows prose, code, and inline layout |
| Line spacing | Auto | Auto or +2 through +10 px | Auto uses content-aware metrics; manual adds a fixed gap |
| Side margins | 5 px | 2-18 px | Changes symmetric document margins and reflows content |
| Tables | Responsive | Responsive, Grid + pan | Chooses compact records or horizontally pannable grid layout |
| Code blocks | Wrap | Pan, Wrap | Chooses horizontal focus/panning or wrapped code |
| Contrast | Standard | Standard, High | Uses the standard or high-contrast palette |
| Text sharpness | Balanced 5 | 0–10 | Runs from extra-smooth grayscale edges to the former Sharpness 7 curve at 10 |
| Touchpad mode | Vertical Scroll | Vertical Scroll, Horizontal Scroll | Chooses the continuous touchpad axis |
| Swipe gesture direction | Natural | Natural, Reversed | Controls discrete page-swipe reading order |
| Scroll gesture direction | Natural | Natural, Reversed | Controls continuous drag reading order |
| Fonts | 0 loaded | Installed files and staged role assignments | Opens the Font Manager |

The guide will explain that layout changes preserve the current source-relative reading anchor rather than an absolute pixel position.

## 7. Persistence and Font Behavior

The guide will distinguish per-document state from global font preferences.

### Persisted per document

Stored in the adjacent `.nmdstate` sidecar:

- Reading position.
- Bookmarks.
- Last selected heading.
- Theme.
- High-contrast setting.
- Font size.
- Line-spacing choice.
- Side margins.
- Table mode.
- Code wrapping.
- Reading mode.
- Scroll behavior.
- Text sharpness.

The guide will provide an example such as `chapter.md.tns.nmdstate` and explain that state is accepted only when it matches the document identity.

### Globally persisted font selection

- Body, companion-style, Monospace, and CJK role assignments remain active while switching documents and across launches.
- Paths are stored in the checksummed `.nmarkdown-fonts` preference below My Documents, not in a document sidecar.
- Body falls back to the minimal built-in printable-ASCII DejaVu UI face.
- Read-only task checkboxes use font-independent native UI primitives.
- Monospace defaults to the embedded printable-ASCII DejaVu Sans Mono face;
  CJK defaults to no external file. Missing italic or bold companions use
  outline oblique or synthetic bold.
- Role assignments reference a unique loaded-file registry; one path used by
  several roles is loaded once.
- Original `.ttf`, `.otf`, and calculator-wrapped `.tns` files are loaded directly.
- No web converter or runtime font-pack format is present.

## 8. Audit Scenarios

### Exit and cancellation

Test `Esc` and `Ctrl+Esc` from:

- Unobscured document.
- Wide-block focus.
- Generic controls.
- TOC.
- Bookmark tab.
- Search.
- Settings.
- Font-family slot menu.
- Font-family choice menu.
- Diagnostics.
- Message dialog.
- Startup file browser.
- In-document file browser.
- Empty and failed-load screens.

Confirm whether each input dismisses exactly one layer, exits, or does nothing.

### Overlay switching

Exercise:

- Menu -> settings.
- Settings -> Menu.
- Search -> settings.
- Settings -> search.
- Diagnostics -> Menu.
- Font selection -> settings.
- Any overlay -> document browser.
- Repeated invocation of the same shortcut.
- `Esc` immediately after each switch.

Record inconsistent close-versus-switch behavior.

### Input leakage

While TOC, settings, and generic controls are open, attempt:

- Vertical touchpad movement.
- Horizontal swipe.
- Plus/minus.
- `Ctrl+B`.
- `Ctrl+N`.
- Left/right.
- Enter.
- Page Up/Page Down.

Compare document position, theme, bookmark state, and page number before and after dismissal.

### Menus and lists

Test:

- Zero, one, six, seven, and many rows.
- First and last row boundaries.
- Page movement near boundaries.
- Long and duplicate filenames.
- Deeply nested heading levels.
- Documents with no headings.
- Documents with headings but no bookmarks.
- Bookmark creation, tab switching, deletion, and restoration.
- Empty document and font directories.
- More files than the configured result cap.

### Search

Test:

- Empty query.
- No results.
- One result.
- More than four results.
- Delete on an empty query.
- Mode changes.
- Result navigation at both boundaries.
- Closing and reopening search.
- `Ctrl+N` after search has closed.
- Searches after document switching.
- Search highlighting after reflow.

### Reading and layout

Test both reading modes with:

- Short and long documents.
- Long filenames competing with the page counter.
- First and last page.
- Dynamic total-page changes after lazy layout.
- Font-size and margin changes while reading.
- Automatic and manual line spacing.
- CJK paragraphs.
- Monospace code.
- Wide tables.
- Overflowing formulas.
- Links near the current viewport anchor.

### Error recovery

Test:

- Invalid Markdown path.
- Oversized source.
- Invalid or oversized font.
- Failed state-file read/write.
- Missing internal anchor.
- Relative Markdown link failure.
- External URL dialog.
- Linked non-Markdown asset.
- Empty document directory.
- Cancelling immediately after launch.

## 9. Detailed Executable Scenario Scripts

This section defines the exact interaction scripts to run. A scenario is not complete merely because the setting label changes. Every option must be exercised in the document, dismissed, used during navigation, reopened, reversed, and checked again.

### 9.1 Test conventions and fixtures

#### Fixture preparation

- Copy `samples/phase4.md` to a disposable file under `build/ui-ux-audit/fixtures/reader.md`. Use this for settings, tables, code, links, search, TOC, bookmarks, and persistence.
- Copy `samples/format-gallery.md` to `build/ui-ux-audit/fixtures/formats.md`. Use this for CJK, monospace, emphasis, mixed text, tables, and formulas.
- Copy `samples/math-complex-review.md` to `build/ui-ux-audit/fixtures/math.md`. Use this for overflowing equations and wide-focus behavior.
- Create a build-only `build/ui-ux-audit/fixtures/interaction-gallery.md` containing at least 15 Latin and CJK headings across levels 1–6, more than ten repeated search terms, composed/decomposed Unicode pairs, at least three links in one paragraph, valid and missing anchors, long code, a wide table, overflowing math, and enough prose to exceed five pages. Use it for TOC, bookmark, search, list-window, and multi-link cases.
- Create `document-a.md` and `document-b.md` as distinct long copies with unique visible markers so cross-document state can be distinguished without relying only on the title.
- Create an empty document directory and an empty font directory under `build/ui-ux-audit/fixtures/empty/` for empty-state tests.
- Use copied documents rather than repository samples for persistence checks so generated `.nmdstate` files remain disposable.
- Start every independent `SET-*`, `NAV-*`, `SRCH-*`, `ERR-*`, and `INPUT-*` case in a fresh process unless the case explicitly tests reopening within one process.
- Remove only disposable sidecars belonging to the copied fixtures before a “factory-default” case. Never delete user documents or user state.

#### Baseline state

Unless a case says otherwise, begin with:

- `reader.md` open at the first line.
- Light theme.
- Standard contrast.
- 15 px body text.
- Automatic line spacing.
- 5 px side margins.
- Responsive tables.
- Code blocks set to Pan.
- Scroll reading mode.
- Built-in ASCII Reading family, synthesized italic/bold styles, and
  Reading-fallback Code.
- No optional CJK face.
- No bookmarks or active search.
- No overlay and no wide block focused.

#### Common settings cycle

Every `SET-*` case must include this complete cycle:

1. Open settings with `Ctrl+T`.
2. Navigate to the named row with the stated number of Up/Down presses.
3. Capture the starting label and selected-row state.
4. Change the option with the specified Left, Right, or Enter input.
5. Capture the changed label while the menu is still open.
6. Close with `Esc` and verify the document is visible rather than the app exiting.
7. Scroll or page once and capture the option working in the document.
8. Reopen with `Ctrl+T` and verify both the value and the retained row selection.
9. Reverse the option with the stated input.
10. Close with `Esc`, scroll or page once, and verify the reverse state in the document.
11. Reopen settings and verify the original value is displayed again.
12. For persisted settings, leave a non-default value selected, close, move to a nonzero reading position, exit normally, relaunch the same document, and verify both the option and source-relative position were restored.

For every step, record the semantic event, physical calculator input, visible label, reading position before/after, and accepted screenshot name. Boolean settings must be tested with Left, Right, and Enter separately because the current UI does not show whether direction matters.

To target settings row `N` deterministically at the beginning of a case, press
`Ctrl+T`, press Up ten times to clamp selection to Theme, then press Down `N`
times. Do not normalize on the first reopen inside the same case: the retained
selected row is itself a required checkpoint. Rows are Theme `0`, Font size
`1`, Line spacing `2`, Side margins `3`, Tables `4`, Code blocks `5`, Contrast
`6`, Reading mode `7`, Scroll behavior `8`, and Fonts `9`.

### 9.2 Settings option scripts

#### SET-01 — Theme: Light and Dark

Primary round trip, matching the requested interaction:

1. Start in Light mode and capture `SET-01-01-light-document.png`.
2. Press `Ctrl+T`; verify `Theme: Light` is selected on row 0. Swipe in all
   four directions and verify neither the selected row nor any option changes.
3. Press Right; verify the complete overlay immediately changes to the dark palette and the label becomes `Theme: Dark`. Capture `SET-01-02-dark-settings.png`.
4. Press `Esc`; verify settings close and the document remains open in Dark mode.
5. Press Down once; verify the document scrolls one line with no palette flash or stale light-colored region. Capture `SET-01-03-dark-scrolled.png`.
6. Press `Ctrl+T`; verify row 0 is still selected and reads `Theme: Dark`.
7. Press Left; verify the label and overlay return to Light.
8. Press `Esc`; press Down once; verify scrolling continues in Light mode. Capture `SET-01-04-light-restored.png`.
9. Reopen settings and verify `Theme: Light`.

Alternate activation and persistence:

10. Press Enter on the Theme row; verify Light changes to Dark exactly once.
11. Hold Enter past the key-repeat delay; verify it does not toggle repeatedly.
12. Close settings, scroll to a recognizable paragraph, exit normally, and relaunch `reader.md`.
13. Verify Dark mode and the recognizable source-relative position are restored.
14. Reopen settings, press Enter to return to Light, close, exit, relaunch, and verify Light persists.
15. From an ordinary document block with no active link or wide focus, press Enter and then click the touchpad. Verify both are inert: theme, scroll position, page, focus, dirty state, and persisted state remain unchanged. Press Down afterward to prove the reader remains responsive.

#### SET-02 — Font size: every value from 12 px through 22 px

Primary round trip:

1. Open settings and press Down once; verify `Font size: 15 px`.
2. Press Right; verify `16 px`, the document reflows behind the overlay, and the selected row remains visible.
3. Close with `Esc`, scroll once, and capture a paragraph at 16 px.
4. Reopen settings; verify row 1 remains selected and shows `16 px`.
5. Press Left; verify `15 px`.
6. Close, scroll once, reopen, and verify `15 px`.

Full range and boundaries:

7. Press Left three times and record the sequence `14`, `13`, `12`.
8. Press Left once more; verify the value remains `12 px`, the anchor does not move, and no redundant visual corruption occurs.
9. Press Right ten times and record every value: `13`, `14`, `15`, `16`, `17`, `18`, `19`, `20`, `21`, `22`.
10. Press Right once more; verify the value remains `22 px`.
11. At 12, 15, and 22 px, close settings, scroll down and up, reopen, and verify the exact value is retained and the same source block remains near the top after reflow.
12. From 22 px, press Left seven times to restore 15 px.

Global shortcut parity and persistence:

13. Close settings. Press Plus once and verify 16 px through a reopened settings panel; press Minus once and verify 15 px.
14. Hold Plus until the maximum and verify it stops at 22 px; hold Minus until the minimum and verify it stops at 12 px.
15. Leave 18 px selected, close, scroll to a recognizable paragraph, exit normally, relaunch, and verify 18 px plus the source-relative position.
16. Restore 15 px and confirm the default layout returns without horizontal clipping.

#### SET-03 — Line spacing: Auto and every manual gap from +2 px through +10 px

Primary round trip:

1. Open settings and press Down twice; verify `Line spacing: Auto`.
2. Press Right; verify `Line spacing: +2 px` and immediate reflow.
3. Close, scroll through prose, a heading, code, and an inline formula, then capture the +2 px result.
4. Reopen settings; verify row 2 and `+2 px` remain selected.
5. Press Left; verify the value returns directly to `Auto`.
6. Close, scroll once, reopen, and verify `Auto`.

Full range and boundaries:

7. Starting from Auto, press Right nine times and record `+2`, `+3`, `+4`, `+5`, `+6`, `+7`, `+8`, `+9`, `+10`.
8. Press Right once more; verify `+10 px` remains unchanged.
9. Press Left nine times and record the reverse sequence down to `Auto`.
10. Press Left once more; verify Auto remains selected.
11. At Auto, +2, +6, and +10, close the menu, scroll across mixed paragraphs and display math, reopen, and verify the value and source anchor.
12. Verify Auto is content-aware rather than displayed as a numeric gap, and that manual values are described as extra spacing rather than total line height.
13. Leave +6 selected, exit and relaunch, verify persistence, then restore Auto.

#### SET-04 — Side margins: every value from 2 px through 18 px

Primary round trip:

1. Open settings and press Down three times; verify `Side margins: 5 px`.
2. Press Right; verify `6 px` and symmetric reflow on both sides.
3. Close, scroll through a wrapped paragraph, and capture the 6 px layout.
4. Reopen; verify row 3 and `6 px`.
5. Press Left; verify `5 px`; close, scroll, reopen, and verify restoration.

Full range and boundaries:

6. Press Left three times and record `4`, `3`, `2`.
7. Press Left again; verify the minimum remains 2 px.
8. Press Right sixteen times and record every value from 3 through 18 px.
9. Press Right again; verify the maximum remains 18 px.
10. At 2, 5, 10, and 18 px, close, scroll through prose/CJK/code/formula content, reopen, and verify the value and source anchor.
11. Verify both margins change equally and the paper still spans the full 320-pixel screen.
12. Verify long words, inline code, tables, and equations remain clipped or wrapped according to their own mode rather than drawing outside the physical screen.
13. Leave 10 px selected, exit and relaunch, verify persistence, then restore 5 px.

#### SET-05 — Tables: Responsive and Grid + pan

Use the table in `reader.md` and begin above the `Comparison` heading.

1. Open settings and press Down four times; verify `Tables: Responsive`.
2. Press Right; verify `Tables: Grid + pan`.
3. Close settings and navigate to the table.
4. Verify columns remain aligned on one line and continuation/focus affordances are visible when the table exceeds the viewport.
5. Press Enter on the table; verify wide focus activates.
6. Press Right three times, then Left three times; verify only the table pans and returns to its initial horizontal position.
7. Press `Esc`; verify wide focus ends without closing the app or changing vertical position.
8. Reopen settings; verify row 4 and `Grid + pan`.
9. Press Left; verify `Responsive`.
10. Close and revisit the same table; verify it becomes narrow-screen records and normal vertical scrolling works without horizontal focus.
11. Press Enter on the responsive table, then press Right. Record whether Enter unexpectedly gives the reformatted table wide focus, whether Right pans it, or whether Right resumes its normal document action; verify the result matches the visible affordance and does not toggle an unrelated setting such as Theme.
12. Reopen settings and verify `Responsive`.
13. Repeat the switch using Enter rather than Right; close, exercise the table, reopen, and switch back using Enter.
14. Repeat using Left from Responsive and record that boolean values toggle rather than follow a directional ordering; flag unclear affordance if the UI gives no cue.
15. Leave Grid + pan selected, exit and relaunch at the table, verify mode and source-relative position, then restore Responsive.

#### SET-06 — Code blocks: Pan and Wrap

Use the long C++ line in `reader.md`.

1. Open settings and press Down five times; verify `Code blocks: Pan`.
2. Close and navigate to the code block.
3. Press Enter; verify wide focus activates on the code block.
4. Press Right four times and verify only the code block moves horizontally; press Left four times and verify it returns.
5. Press `Esc`; verify focus ends and vertical reading position is unchanged.
6. Reopen settings at row 5 and press Right; verify `Code blocks: Wrap`.
7. Close and return to the same code block; verify the long line wraps and no horizontal continuation indicator remains.
8. Press Enter and Right; record the actual focus/pan behavior. Static inspection predicts that a code block remains wide-focusable because every code background qualifies even when Wrap is enabled; compare that behavior with the purpose and visible affordance of Wrap rather than presuming focus is unavailable.
9. Scroll past and back to the code block; verify wrapped layout remains stable.
10. Reopen settings; verify `Wrap`; press Left to return to `Pan`.
11. Close, scroll, re-enter focus, and verify panning works again.
12. Repeat Pan → Wrap → Pan with Enter on the settings row.
13. Leave Wrap selected, exit and relaunch at the code block, verify persistence and anchor restoration, then restore Pan.

#### SET-07 — Contrast: Standard and High in both Light and Dark themes

Light-theme round trip:

1. Confirm Theme is Light.
2. Open settings and press Down six times; verify `Contrast: Standard`.
3. Press Right; verify `Contrast: High` and capture the selected and unselected settings rows.
4. Close, scroll through body text, links, code, math, and the top progress bar; capture the light/high-contrast document.
5. Reopen; verify `High`; press Left; verify `Standard`.
6. Close, scroll, reopen, and verify `Standard`.

Dark-theme cross-product:

7. Move to Theme row, switch to Dark, return to Contrast row, and set High.
8. Close and inspect dark/high-contrast document, overlay, selection highlight, code background, link underline, search highlight, progress bar, and error color.
9. Reopen and switch Contrast to Standard; close and inspect dark/standard.
10. Restore Light/Standard.
11. For all four combinations—Light/Standard, Light/High, Dark/Standard, Dark/High—verify selected text remains legible, muted text remains distinguishable, and state is not conveyed by color alone.
12. Leave Dark/High selected, exit and relaunch, verify both settings persist together, then restore Light/Standard.

#### SET-08 — Reading mode: Scroll and Page Swipe

Scroll baseline:

1. Open a document long enough for at least three viewports and return to its exact top.
2. In Scroll mode, verify no page number is drawn in the upper-right header.
3. Capture the top frame; verify the neutral two-pixel track spans the screen and the colored reading fill is exactly zero pixels wide (0%).
4. Press Down once; verify an approximately 18-pixel line scroll rather than a full page.
5. Capture again; verify the colored fill is now partial and equals `floor(320 × scroll offset / maximum scroll offset)`.
6. Press Shift+Down and Ctrl+Down separately; verify each performs a page-sized movement and each increases the fill continuously from its prior width.
7. Perform a vertical touchpad drag; verify continuous movement and a corresponding continuous progress change rather than a page-sized jump.
8. Move to the exact bottom; capture and verify the colored fill reaches pixel 319, for a full 320-pixel (100%) width.
9. Return to the exact top and verify the fill returns to zero rather than retaining a minimum-width marker.

Switch to Page Swipe:

10. Open settings and navigate to row 7; verify `Reading mode: Scroll`.
11. Press Right; verify `Page swipe` and that the current position aligns to a complete reading page.
12. Close on page 1; verify `1 / total` appears at the top right without colliding with the filename and the colored fill remains exactly zero pixels wide.
13. Advance exactly one page; verify the new page begins on a complete line and the counter reads `2 / total`.
14. Capture page 2 and verify the fill is `floor(320 × (2 - 1) / (total - 1))`; for exactly three pages this must be 160 pixels, not 213 or 214.
15. Return one page; verify page 1 and the zero-width fill return together.
16. Press Right and Left once each; verify next/previous page behavior and that the fill changes only by whole page positions.
17. Swipe left and right once each; verify next/previous page behavior, direction, counter, and discrete fill all agree.
18. Drag vertically; verify at most one discrete page transition per reported event and no partial-page resting position.
19. Move to the final page; capture and verify the counter reads `total / total` and the fill reaches the full 320-pixel width.
20. Reopen settings; verify row 7 and `Page swipe`.

Reverse to Scroll:

21. Press Left; verify `Scroll`.
22. Close; verify the page number disappears, the two-pixel track remains, and its fill is recomputed from the continuous scroll offset rather than copied from the former page fraction.
23. Press Down and verify line scrolling resumes and changes progress continuously; press Left/Right and verify they do not change pages unless a wide block is focused.
24. Reopen and verify `Scroll`.
25. Repeat Scroll → Page Swipe → Scroll using Enter on the settings row; at every switch, capture the frame and verify the progress formula changes with the selected mode.
26. Leave Page Swipe selected on page 2 or later, exit and relaunch, and verify the mode, page label, aligned source-relative position, and corresponding discrete progress; then restore Scroll.
27. Open a document that fits in one viewport. Verify its Scroll progress is 0%, switch to Page Swipe, verify `1 / 1` and 0% again, then switch back.

#### SET-09 — Scroll behavior: Natural and Traditional

Scroll-mode vertical drag:

1. Open a document long enough for several viewports, return to its exact top,
   open Settings, and navigate to row 8; verify `Scroll behavior: Natural`.
2. Close Settings and drag upward continuously. Verify the document advances
   toward later content, the progress bar advances, and the top text remains
   responsive after release.
3. Start a new contact and drag downward by the same distance. Verify the
   document returns toward earlier content.
4. Press Up, Down, 8, and 2. Verify their earlier/later meanings are unchanged
   by Natural scrolling. Tap and click ordinary prose and verify neither input
   changes the setting or reading position.
5. Reopen Settings; verify row 8 is retained. Press Enter and verify the label
   becomes `Scroll behavior: Traditional`. Close Settings.
6. Drag downward continuously and verify the document advances; use a new
   upward drag to return. Repeat the toggle with Left and Right separately and
   verify each changes the value exactly once.
7. In both values, swipe right once and left once in Scroll mode. Verify these
   discrete page-sized gestures retain their existing directions and each new
   viewport begins on a complete text-line top.

Page-Swipe horizontal drag:

8. Select Page Swipe, return to page 1, set Scroll behavior to Natural, and
   close Settings. Drag left continuously; verify the reading position advances
   toward later content. Start a new rightward drag and verify it returns.
9. Set Scroll behavior to Traditional. Drag right continuously; verify it now
   advances. Start a new leftward drag and verify it returns.
10. In both values, use Up/Down, 8/2, Left/Right, Page Up/Down, and one discrete
    vertical swipe in each direction. Verify none of those mappings changes.
11. Enter wide focus on code, a grid table, and an oversized formula. Drag
    horizontally and verify the focused canvas continues to follow the finger;
    Scroll behavior must not turn local panning into document navigation.

Persistence and isolation:

12. Leave Traditional selected, close Settings, move to a recognizable source
    position, exit normally, and relaunch the same document. Verify both the
    source-relative position and `Scroll behavior: Traditional` are restored.
13. Restore Natural, exit, relaunch, and verify Natural persists. Open every
    modal overlay and perform continuous drags; verify the covered document
    remains stationary and the option is not changed by modal navigation.

#### SET-10 — Font Registry and Role Assignments

Prepare separate DejaVu Sans Regular/Oblique/Bold/Bold Oblique files, DejaVu
Sans Mono Regular/Oblique, a proportional Latin+CJK font, a full fixed
Latin+CJK Sarasa face, a CJK-only subset, a variable font, and malformed font
lookalikes in nested folders below disposable My Documents. Record each valid
file's OpenType names, style bits, PANOSE, `post.isFixedPitch`, cmap coverage,
variable axes, byte size, and metadata records. Capture a calculator-screen
screenshot after every numbered run.

Open, scan, and cancel:

1. Cold-launch with no external assignments → open Settings → press Down nine
   times → capture `Fonts: 0 loaded` → scroll the settings list → verify the
   selected row remains fully visible and no document content moves.
2. Press Enter on Fonts → if scanning is slow, capture `Finding fonts` and its
   progress detail → capture **Installed fonts** → verify individual files, not
   three role pickers or grouped families, are listed.
3. Select DejaVu Sans Regular → capture its detail page → verify `Detected:
   Latin proportional`, seven role checkboxes, **Use suggested roles**, and
   **Unload from roles** are readable at the larger menu font size.
4. Press Esc → capture Installed fonts with no assignment applied → press Esc →
   capture the unobscured document → scroll → press `Ctrl+Esc` in a separate
   run; verify cancel, responsiveness, and global exit.
5. Reopen Fonts in the same process → capture the immediate cached list → verify
   no second scan/loading interval and no full-font `read_all` calls.
6. Open a detail page → toggle Body → press Esc to the list → press Esc instead
   of Apply → reopen the same detail → capture unchecked Body; verify top-level
   Esc discarded the staged map.

Detection and suggestions:

7. Open DejaVu Sans Regular → capture Body marked suggested → choose **Use
   suggested roles** → capture Body checked and all unrelated roles unchecked.
8. Open DejaVu Sans Oblique → capture italic detection and Body Italic
   suggestion → use suggestions → capture the staged companion assignment.
9. Repeat run 8 with Bold and Bold Oblique → capture Body Bold and Body Bold
   Italic suggestions separately; verify weight/style bits do not suggest Body.
10. Open DejaVu Sans Mono Regular → capture fixed-pitch detection and Monospace
    suggestion → open Mono Oblique → capture Monospace Italic suggestion.
11. Open full Sarasa Fixed SC → capture Latin, CJK, and fixed detection → use
    suggestions → capture Monospace and CJK checked together.
12. Open proportional Latin+CJK → capture Body and CJK suggestions → use them →
    verify Monospace remains unchecked.
13. Open the CJK-only subset → capture CJK but not Latin detection → verify it
    suggests CJK only and does not become a Body/Monospace suggestion merely
    because punctuation or Latin-1 symbols remain in cmap.
14. Open the variable font → capture the `variable` capability → manually
    override every suggested role once; verify metadata never disables a role.

Apply, rendering, and fallbacks:

15. Stage DejaVu Regular as Body and Oblique as Body Italic → select **Apply
    changes** → capture the first document frame → scroll → reopen the manager;
    verify one reflow, no white frame, and the same source-relative anchor.
16. Open ordinary/emphasized Latin at 12, 15, and 22 px → capture each → verify
    the real oblique face supplies italics without a second synthetic slant.
17. Unassign Body Italic → apply → capture the same samples → verify FreeType's
    outline oblique fallback, not bitmap-row shearing.
18. Repeat runs 15–17 for Body Bold/Body Bold Italic → verify native faces when
    assigned and synthetic bold/oblique only for missing companions.
19. Assign DejaVu Sans Mono to Monospace and Mono Oblique to Monospace Italic →
    apply → capture inline, wrapped, focused, and panned code → verify prose
    stays on Body and code interactions remain responsive.
20. Clear Monospace while keeping CJK assigned → apply → capture Latin code and
    CJK code → verify `Monospace → built-in DejaVu Sans Mono → CJK`: printable
    ASCII remains fixed-width in the embedded face while CJK reaches the
    separately assigned family.
21. Clear Body while keeping CJK → apply → capture mixed prose → verify
    `Body → UI → CJK`; then clear CJK and capture replacement glyphs without a
    crash or invisible modal state.
22. Capture formulas before and after all text-role changes → verify embedded
    Latin Modern Math remains unchanged and absent from the assignable list.

One file, several roles, and memory identity:

23. Open full Sarasa → check Body, Monospace, and CJK manually → capture all
    three checkmarks → apply → capture mixed prose/code/CJK.
24. Inspect runtime instrumentation after run 23 → verify the three role IDs are
    equal, the registry contains one resource, one `FT_Face`, one HarfBuzz font,
    and one glyph-cache identity, and the file's bytes count once.
25. Reopen Sarasa → uncheck Body only → apply → verify Monospace/CJK keep the
    same resource ID and no file reload occurs.
26. Reopen Sarasa → choose **Unload from roles** → capture every role unchecked →
    apply → verify the resource is released but the font file remains visible
    under Installed fonts and remains present in My Documents.

Discovery, limits, and invalid inputs:

27. Distribute fonts through nested My Documents folders → cold-open Fonts →
    capture the list → verify no dedicated directory, hidden entries skipped,
    same-family files remain separate rows, and sorting is stable.
28. With more than six files, use Up/Down, modified Up/Down, and swipe from top
    to middle to bottom → capture each → verify selection remains visible.
    Short-tap the selected file, role, and action → verify each is inert. Then
    physically click each → verify the selected action is activated exactly
    once.
29. Hit the 2,048-entry, 128-font, and depth-12 bounds separately → capture
    `Font search stopped early` → attempt activation → verify it is inert → Esc
    → scroll the document → verify no freeze.
30. Include invalid signatures, truncated SFNT directories, malformed names,
    unsupported collections, and unreadable paths → capture the valid list →
    verify bad files are skipped without aborting discovery.
31. Instrument filesystem access → open Fonts → verify bounded `probe` and
    `read_range` metadata reads only → stage several assignments → verify no
    payload read yet → choose Apply → verify one `read_all` per unique path.
32. Assign the same large path to several roles near the aggregate limit →
    apply → verify it is admitted once; then add a distinct file over the limit
    → capture `Could not apply fonts` → dismiss with Enter and Esc in separate
    runs → verify the old registry, anchor, scrolling, and `Ctrl+Esc` still work.

Persistence and migration:

33. Apply all seven roles with deliberate shared paths → open another document
    with `Ctrl+O` → capture its first frame → verify assignments remain active
    without scanning or duplicate loads.
34. Exit → relaunch → capture the first document frame before opening Fonts →
    inspect `.nmarkdown-fonts` → verify `NMF3`, seven role paths, duplicate paths
    preserved as references, and each unique file loaded once.
35. Seed valid `NMF1` and `NMF2` preferences in separate runs → launch → capture
    restored roles and safe empty newer companions → apply once → verify the
    rewritten preference is `NMF3`.
36. Move one remembered file → relaunch → capture safe built-in fallback and a
    responsive document → open Fonts → reassign an available file → apply →
    verify the preference is repaired.
37. Corrupt `.nmarkdown-fonts` → relaunch → capture safe defaults → verify no
    font scan occurs until Fonts is explicitly opened.
38. Remove all external font files → open Fonts → capture only **Apply changes**
    (plus a boundary warning if deliberately triggered) → exercise Esc, Apply,
    scrolling, and exit; verify no converter, import prompt, or font-pack option
    appears anywhere.

### 9.3 Navigation, overlay, and input scripts

#### NAV-01 — Esc back-stack from every state

For each state below, start a fresh process, open the state, press `Esc` once, and verify the exact result before pressing anything else:

| Starting state | Expected first Esc result | Expected second Esc result |
|---|---|---|
| Document, no overlay | Exit to TI-OS | Not applicable |
| Wide code/table/formula focus | Leave focus and reset horizontal pan | Exit only after focus is gone |
| Generic controls | Close controls, preserve document position/theme | Exit from document |
| TOC | Close TOC, preserve document position | Exit from document |
| Bookmark tab | Close the whole TOC/bookmark overlay | Exit from document |
| Search | Close search, retain query/results for later `Ctrl+N` | Exit from document |
| Settings | Close settings and retain live changes | Exit from document |
| Font-family choice list | Return to family-slot list | Close font UI without exiting |
| Font-role list | Close font UI | Exit from document |
| Diagnostics | Close diagnostics | Exit from document |
| Message/error dialog | Close dialog | Exit only if no other state remains |
| In-document browser | Cancel and return to the same document/position | Exit from document |
| Startup browser | Exit to TI-OS because no document is behind it | Not applicable |
| Failed startup document | Exit to TI-OS | Not applicable |

Capture before Esc, after first Esc, and after any second Esc. Require a normal exit marker and restored TI-OS framebuffer for actual exit cases.

#### NAV-02 — Ctrl+Esc immediate exit

1. Repeat every starting state in NAV-01.
2. Press `Ctrl+Esc` once without first dismissing the state.
3. Verify the app exits immediately, emits its normal exit marker, restores the display, and does not require another key.
4. Relaunch the copied document and verify state saving is coherent: per-document settings/bookmarks/position and global font paths are restored from their independent stores.
5. Verify holding Ctrl+Esc does not launch, close, or reactivate TI-OS content after the app has exited.

#### NAV-03 — Shortcut switching between overlays

For every transition below, capture the source overlay, invoke the destination shortcut, capture the destination, press Esc, and verify exactly one overlay was active and the document returns without exiting:

- TOC → `Ctrl+T` → Settings.
- Settings → Doc → TOC or generic controls.
- TOC → `Ctrl+F` → Search.
- Search → `Ctrl+T` → Settings.
- Settings → `Ctrl+F` → Search.
- Search → Doc → TOC or generic controls.
- Settings → `Ctrl+D` → Diagnostics.
- TOC → `Ctrl+D` → Diagnostics.
- Diagnostics → Menu and Diagnostics → `Ctrl+D`.
- Any top-level overlay → `Ctrl+O` → document browser.
- Font-role list → `Ctrl+T` and Font-role list → Menu.
- Font-file list → `Ctrl+T` and Font-file list → Menu.

Repeat the same shortcut while its own overlay is open. Record whether it toggles closed, refreshes, resets selection, or is ignored. Flag inconsistent behavior when equivalent top-level overlays follow different rules.

#### NAV-04 — Startup document browser

1. Launch without a document in a directory containing at least eight Markdown files.
2. Verify `Open Markdown` appears, row 0 is selected, basenames are sorted case-insensitively, and no stale document is visible behind it.
3. Press Down seven times and verify the selected file remains visible as the six-row window scrolls.
4. Press modified Up/Down and verify five-row movement with correct boundary clamping.
5. Press Enter on a valid document; verify the browser closes, the selected file opens, its filename appears in the header, and reading begins at its saved position if valid state exists.
6. Relaunch to the startup browser and press Esc; verify immediate normal exit to TI-OS.
7. Relaunch and press Ctrl+Esc; verify the same immediate exit contract.
8. Launch against the empty fixture directory; verify `No Markdown files found`, Enter does nothing, and Esc exits normally.
9. Include duplicate basenames in different folders and record whether the browser gives enough path context to distinguish them.
10. Include more than 256 matches and verify the cap is stable and any absence of cap feedback is recorded.

#### NAV-05 — In-document document browser

1. Open `reader.md`, move to a recognizable paragraph, and record its source anchor.
2. Press `Ctrl+O`; verify the browser overlays the current document.
3. Move selection, press Esc, and verify the original document, anchor, theme, settings, and fonts are unchanged.
4. Scroll once to prove the reader remains responsive.
5. Reopen with `Ctrl+O`, select `formats.md`, and press Enter.
6. Verify the title changes, the new document renders, the previous document state is saved, and global fonts remain active.
7. Press `Ctrl+O`, return to `reader.md`, and verify its source-relative position and per-document settings restore.
8. While the browser is already open, press `Ctrl+O` again; record whether it closes, refreshes, or resets selection, then Esc and verify recovery.
9. Attempt to open an invalid/oversized file and verify an error dialog appears over the still-valid prior document.

#### NAV-06 — Table of contents repeated and reverse jumps

1. Open a document with at least eight headings and begin at the top.
2. Press Doc; verify TOC opens with the persisted or first heading selected.
3. Press Down once, capture selection, press Enter, and verify the second heading is at the reading position.
4. Scroll two lines, reopen TOC, press Down once, activate the next heading, and verify a distinct forward jump.
5. Reopen TOC, press Up once, activate, and verify a backward jump.
6. Reopen a fourth time and press Esc; verify the overlay closes and the document remains responsive.
7. Reopen, press modified Down, verify a five-row jump and visible selection; press modified Up and verify reversal.
8. Hold Down through repeat delay and verify repeat timing, boundary clamping, and no freeze.
9. Exit and relaunch; reopen TOC and verify the last selected heading restores from document state.
10. Run the existing strict Firebird Alpha → Beta → Alpha trace and require at least the existing frame/milestone count.

#### NAV-07 — Bookmarks add, navigate, remove, and persist

1. With no bookmarks, press Doc and then Right; verify whether tab switching is unavailable and whether that is understandable.
2. Close, move to a recognizable block, press `Ctrl+B`, and record visible feedback or its absence.
3. Move to two later blocks and add two more bookmarks.
4. Open Menu, press Right, and verify the Bookmarks title/tab state and three source-ordered entries.
5. Press Down, Enter, and verify navigation to the second bookmark plus overlay dismissal.
6. Reopen bookmarks, use modified Down/Up, and verify boundary clamping.
7. At a bookmarked source block, press `Ctrl+B` to remove it; reopen and verify the entry disappears without corrupting selection.
8. Remove all bookmarks; reopen with Doc and verify TOC remains accessible and the empty bookmark state cannot trap focus.
9. Add two bookmarks, exit and relaunch, open Bookmarks, and verify both persist and navigate after reflow caused by a font-size change.

#### NAV-08 — Reading-key aliases, activation, and Scratchpad

Use a document long enough for at least four pages, with one overflowing code
block and at least eight headings. Start each subsection from a fresh process
and capture the framebuffer after every numbered input that changes visible
state.

Scroll-mode equivalence:

1. Start at the top in Scroll mode, press Down once, and record the resulting
   source anchor and scroll offset.
2. Relaunch at the top, press 2 once, and verify the same anchor/offset and a
   rendered frame; press Down afterward to prove direct transitions between
   equivalent physical keys are not suppressed.
3. Repeat the preceding comparison for Up versus 8 after first moving down two
   lines.
4. From the top, press Tab once and record the page-sized destination. Relaunch
   and repeat with 1; verify the exact same destination. Press 7 and verify the
   prior Page Up destination returns.
5. Hold Tab, then 1, then 7 in separate runs. Verify the first repeat occurs no
   earlier than approximately 360 ms, subsequent repeats occur approximately
   every 70 ms, and movement clamps cleanly at both document boundaries.
6. Press Left/4 and Right/6 on ordinary Scroll content; verify each alias pair
   has the same no-op behavior and does not mark the document state dirty.
7. Focus the overflowing code block. Compare Left with 4 and Right with 6 from
   the same pan offset; verify identical 12-pixel local panning, conventional
   focused-canvas directions, edge clamping, and no movement of the document
   underneath.

Page-Swipe equivalence:

8. Switch to Page Swipe and start on page 2. Compare Up with 8, Down with 2,
   Left with 4, and Right with 6 in four fresh runs. Verify each alias reaches
   the same complete page as its named arrow under the configured reversed
   reading direction.
9. From page 2, compare Page Down with Tab and with 1; compare Page Up with 7.
   Verify the page label, source anchor, and top progress strip match for each
   equivalent input.
10. On an overflowing formula in Page Swipe, verify 4/6 retain Left/Right local
    panning and 7/Tab/1 retain Page Up/Page Down local viewport panning until
    Esc leaves focus; then verify the same keys resume document paging.

Overlay and text-entry equivalence:

11. Open Settings. Verify 8/2 selects the same prior/next row as Up/Down and
    4/6 changes the selected value exactly like Left/Right. Close and confirm
    the chosen value persisted once, without an extra action caused by repeat.
12. Open the TOC on its first row. Verify Tab and 1 move down by the same
    multi-row amount as Page Down, while 7 moves back by the Page Up amount;
    activate a heading and prove the document remains responsive.
13. Open Search. Verify every digit, including 1/2/4/6/7/8, enters query text
    without moving result selection or changing modes. Verify Up/Down still
    moves one result, modified Up/Down and Tab move several results, and
    Left/Right switches modes. Close Search and verify the numeric aliases
    immediately resume their documented reader-navigation behavior.

Activation and browser routing:

14. On ordinary prose with no link, list action, or overflowing target, record
    theme, scroll/page, focus, dirty state, and pending state-save status. Press
    Enter, then repeat from a fresh state with touchpad click. Verify both
    return no action, draw no changed frame, and leave every recorded value
    unchanged; press Down to prove liveness.
15. Repeat Enter on a link, TOC row, Settings row, and overflowing formula.
    Verify these actionable contexts still activate exactly once.
16. From a document, press Scratchpad; verify the Markdown browser opens with
    the same document list and selection rules as Ctrl+O. Press Esc and verify
    the original document, anchor, settings, and session fonts are unchanged.
17. Reopen with Scratchpad and press Scratchpad again; verify the browser closes
    according to the same toggle/cancel rule as Ctrl+O and the reader remains
    responsive. Hold Scratchpad beyond 1 second and verify it does not repeat.
18. Launch directly into the startup browser and press Scratchpad; verify it
    cancels safely to TI-OS because no document exists behind it.
19. Test simultaneous-key priority in separate runs: Esc+1 must perform Back,
    Ctrl+Esc+1 must quit, and Ctrl+O+2 must open the document browser without a
    stray line movement. Capture the final document/browser frame and require
    a normal exit marker after each run.

Harness coverage note: run raw Ndless input tests for all physical constants.
PocketJS currently exposes Tab and `n2`, so the native Firebird run must at
least prove Tab Page Down, `n2` Down equivalence, inert Enter, a rendered frame
after each, and final reader liveness. Record 1/4/6/7/8/Scratchpad as a harness
injection limitation unless PocketJS gains those key names; do not substitute a
different physical key and claim full-device coverage.

#### SRCH-01 — Search entry, modes, results, close/reopen, and next

1. Press `Ctrl+F`; verify the query prompt, default ASCII-fold mode, zero-match status, and visible input focus.
2. Type `comparison`; verify the query updates once per key and results update without freezing.
3. Press Down and Up across the result set; verify selection remains visible and clamps at both ends.
4. Use modified Down/Up to move four results where enough matches exist.
5. Press Enter; verify navigation to the selected source match, search closes, and the match is highlighted.
6. Scroll once, press `Ctrl+F`, and verify the prior query and selected mode are retained.
7. Press Esc; verify search closes without clearing the query; press `Ctrl+N` twice and verify results advance and wrap at the end.
8. Reopen search and press Delete until empty; press Delete once more and verify no corruption.
9. Enter a query with no results; verify `0 matches`, Up/Down/Enter are safe, and Esc closes.
10. Cycle Right through ASCII fold → canonical → Unicode fold → exact → ASCII fold, verifying title and result updates at every step; cycle Left through the reverse order.
11. Type uppercase letters with Shift, every digit, space, and period. Verify
    1/2/4/6/7/8 append to the query without moving the result selection or
    changing the search mode; document all other calculator characters that
    cannot be entered.
12. Attempt a query beyond 64 bytes and verify additional input is ignored without breaking UTF-8.
13. Switch documents after a search and verify stale results cannot navigate into the previous document.

#### NAV-09 — Generic controls overlay for a document without headings

1. Open a Markdown document containing prose but no headings or bookmarks.
2. Press Doc; verify the generic `Reader controls` overlay appears rather than an empty TOC.
3. Press Esc; verify it closes and the document does not exit.
4. Reopen, press Enter, and verify it is consumed as an inert action: the
   controls remain visible and the theme and document state do not change.
5. Reopen and test Up/Down, modified Up/Down, Left/Right, vertical drag, and horizontal swipe; verify whether any input leaks to the document behind the overlay.
6. Press Doc while it is open and verify it closes consistently with TOC behavior.

#### NAV-10 — Diagnostics and informational/error dialogs

Diagnostics:

1. Press `Ctrl+D`; verify all diagnostic rows fit within the panel and counters are readable.
2. Press Esc; verify the panel closes and the document remains open.
3. Reopen and press `Ctrl+D`; verify the same shortcut closes it.
4. Reopen and press Menu; verify Diagnostics is replaced by exactly one Reader
   Settings overlay, then press Esc and verify the document returns.
5. Reopen after scrolling/rendering and verify counters refresh rather than remaining stale.

Dialogs:

6. Activate an external link; verify a view-only dialog shows the URL and `Enter or Esc closes`.
7. Close with Enter; repeat and close with Esc; repeat and close with Menu.
8. Trigger missing anchor, failed relative link, linked asset, invalid font, and document-open errors; verify each title/message identifies the problem and preserves the prior valid document.
9. From each dialog press `Ctrl+Esc`; verify immediate exit.
10. Try `Ctrl+T`, `Ctrl+F`, and `Ctrl+O` while a dialog is open and record whether they switch state or are ignored consistently.

#### NAV-11 — Wide code, table, and formula focus

For a long code block, Grid table, and overflowing formula separately:

1. Align the block near the top of the viewport and record vertical position.
2. Press Enter; verify focus/continuation indication appears and horizontal pan starts at zero.
3. Press Right until the content reaches its maximum pan; press Right once more and verify clamping.
4. Press Left until zero; press Left once more and verify clamping.
5. Hold Right and Left to verify key repeat and stable rendering.
6. Press Enter again and record whether focus toggles off; refocus, pan, then press Esc and verify focus closes plus pan resets.
7. Press Up/Down while focused and record whether vertical scrolling is intentional and understandable.
8. Open an overlay while focused, close it, and verify focus/pan state is either preserved or deliberately reset without leaving a cursor artifact.

#### INPUT-01 — Overlay input isolation

Run this sequence for Generic controls, TOC, Bookmarks, Settings, Search,
Diagnostics, document browser, font-family slot list, font-family choice list,
and a message dialog:

1. Record document source anchor, page number, horizontal pan, theme, font size, bookmark count, search selection, and active overlay.
2. Open the target overlay.
3. Perform a small vertical touchpad drag.
4. Perform left and right horizontal swipes.
5. Press Plus, Minus, `Ctrl+B`, `Ctrl+N`, Enter, modified Up/Down, Left, and Right one at a time, restoring the start state between inputs.
6. Close the overlay with Esc.
7. Compare every recorded value.
8. Treat any undocumented document movement, page change, theme change, reflow, bookmark mutation, or search jump behind a modal as an input-leak finding.
9. Repeat in Scroll and Page Swipe modes.

#### INPUT-02 — Key repeat and simultaneous-modifier priority

1. Hold Up, Down, Left, Right, and Delete separately; verify no repeat before approximately 360 ms and repeats near 70 ms thereafter.
2. Verify Enter, Menu, Plus, Minus, and shortcut commands do not repeat while held.
3. Test Shift+Up/Down and Ctrl+Up/Down in document, TOC, search, font list, and file list contexts.
4. Test `Ctrl+Esc` while another Ctrl shortcut key is also pressed; verify quit has priority.
5. Test touchpad movement while a physical key is held; verify the active key suppresses conflicting touch events.
6. Record simulator limitations when PocketJS cannot express a simultaneous key combination; do not add production aliases solely for the audit.

#### INPUT-03 — Touchpad deadzones and axis locking

Run this sequence in Scroll mode and repeat it in Page Swipe mode:

1. Touch the center, move less than 1.25% of the shorter native coordinate span, and release within 500 ms. Verify exactly one tap activation and no drag or swipe.
2. Touch the center, move beyond the tap slop but less than the approximately 4% drag-start threshold, and release. Verify no activation, drag, or swipe.
3. Move diagonally with nearly equal physical-scale X/Y travel. Verify no axis is selected and releasing does not activate the current item.
4. Start a horizontal drag with small vertical jitter until horizontal travel leads by at least 4:3. Verify horizontal events are emitted.
5. Before reaching the named-swipe threshold, move much farther vertically than horizontally. Verify the provisional lock corrects once to vertical rather than swallowing the gesture.
6. Cross the named-swipe threshold, then move much farther along the other axis. Verify the committed axis remains locked and no orthogonal event leaks through. Release and repeat with the axes exchanged.
7. After locking an axis, move by less than the continuous-motion deadzone (approximately 1.25% of the shorter native span). Verify no event. Continue slowly in the same direction until the accumulated movement crosses the deadzone; verify one continuous event contains the accumulated delta.
8. Start a fresh contact in each cardinal direction. Cross 20% travel along the locked axis and verify exactly one corresponding named swipe per contact: Left, Right, Up, and Down.
9. Repeat the ambiguous diagonal and both dominant-axis paths using Firebird's 100 × 70 touch surface and 2328 × 1691 native coordinate ranges. Verify physical angles are preserved instead of stretching the rectangular pad into a square.
10. Perform a slow drag close to an edge, release, and immediately begin an opposite-axis drag. Verify release resets all origin, deadzone, and axis-lock state.

#### STATE-01 — Per-document state versus session state

1. In `reader.md`, choose Dark, 18 px, +6 line spacing, 10 px margins, Grid table, Wrap code, High contrast, Page Swipe, add two bookmarks, select a later TOC heading, and move to page 2 or later.
2. Assign custom Body, companion-style, Monospace, and CJK roles when
   available; deliberately share one file across Monospace and CJK and verify
   the registry loads it once.
3. Open `formats.md` with `Ctrl+O`; verify fonts carry across immediately and record whether settings inherit before any `formats.md` sidecar exists.
4. Set visibly different options in `formats.md`, move to a distinct position, and add one bookmark.
5. Return to `reader.md`; verify its complete persisted setting/bookmark/position set restores.
6. Return to `formats.md`; verify its distinct state restores.
7. Exit and relaunch each document directly; verify document state and global font-role assignments persist independently.
8. Modify the copied Markdown content so its identity changes, relaunch, and verify stale source-relative state is rejected safely.
9. Corrupt the disposable sidecar checksum and verify the document still opens with safe defaults/current session state rather than freezing.

#### STATE-02 — Individual A/B persistence cycle for every persisted setting

Run this as eight independent cases with clean disposable sidecars. Do not rely only on the combined STATE-01 case.

| Setting | Document A value | Document B value |
|---|---|---|
| Theme | Dark | Light |
| Font size | 18 px | 13 px |
| Line spacing | +6 px | Auto |
| Side margins | 10 px | 3 px |
| Tables | Grid + pan | Responsive |
| Code blocks | Wrap | Pan |
| Contrast | High | Standard |
| Reading mode | Page Swipe | Scroll |

For each row in the table:

1. Start with fresh A and B sidecars and open A.
2. Set only A's named value, close settings, scroll to marker `A-2`, and verify the value in use.
3. Open B with `Ctrl+O`; this saves A. Verify a sidecar exists for A and record whether brand-new B inherits A's current live setting before B has state of its own.
4. Set B's named value, close settings, scroll to marker `B-2`, then return to A with `Ctrl+O`.
5. Verify A restores its named value and source marker rather than B's value.
6. Return to B and verify B's distinct value and source marker.
7. Exit normally, relaunch A directly, and verify A again.
8. Relaunch B directly and verify B again.
9. Reopen settings after every restoration, then close and perform one navigation input to prove liveness.
10. Record state inheritance for new documents as current behavior; do not describe new documents as automatically resetting to factory defaults unless observation proves it.

#### STATE-03 — Sidecar rejection, silent fallback, and save timing

Use only disposable fixture sidecars.

1. Create valid state, then test truncated, invalid-signature, bad-checksum, unsupported-version, stale-identity, unreadable, and unwritable sidecars separately.
2. For each, launch the document and record the effective settings/position and whether any user-visible explanation appears. Static inspection predicts silent fallback for state-read/decode failures and silent failure for unsuccessful state writes.
3. After fallback, open settings, scroll, open/close TOC, and exit to prove the reader is still live.
4. Change a setting and position, switch documents, and verify the state is saved at document switch.
5. Change another setting and position, exit with Esc, relaunch, and verify normal-exit saving.
6. Repeat with semantic `Quit`/`Ctrl+Esc`, relaunch, and verify the normal loop still saves before returning.
7. Simulate an abnormal termination only in the disposable desktop environment and verify changes since the last switch/normal exit may be lost; document this save-timing limitation without implying crash-safe autosave.
8. Verify bookmarks, TOC selection, source-relative position, and rows 0–7 are included, while font choices and search query/mode are not stored in `.nmdstate`.

### 9.4 Current implementation risk hypotheses to reproduce

These are source-grounded hypotheses, not final audit findings. Each must be reproduced or explicitly marked unconfirmed:

- If wide focus exists underneath Settings, TOC, or Reader controls, the first Esc may clear wide focus while leaving the visible overlay open, because wide-focus Back handling precedes general overlay dismissal.
- Settings and TOC can allow Page Up/Down or touchpad `PointerScroll` to move the document behind the visible overlay.
- Plus/Minus, `Ctrl+B`, and stored `Ctrl+N` actions can mutate font size, bookmarks, or reading position behind some overlays.
- Reader controls can pass document actions through and may not self-toggle with Menu like TOC does.
- Overlay shortcut routing is asymmetric: some overlays switch to a requested destination, while Search may ignore Settings/Menu/Diagnostics, Diagnostics may merely close on Menu, and Fonts may close rather than return to Settings.
- `Ctrl+O` is intercepted by the application before Viewer routing; repeated `Ctrl+O` while the browser is open may rebuild it and reset selection rather than toggle it closed.
- Enter has no visible link focus; it scans the current top block and activates its first link, making multiple links within one block ambiguous.
- Code blocks may remain wide-focusable after Code blocks is set to Wrap.
- Search query and mode can survive closing Search and switching documents even though results are rebuilt for the new document.
- A new document without a valid sidecar can inherit the current Viewer's live settings; state errors and write failures can be silent.
- State is written at document switch or normal exit, not continuously after every setting/bookmark/scroll change.
- Document and font pickers are flat recursive lists: hidden names are skipped, recursion stops after depth 12, display uses basename only, and duplicate basenames are indistinguishable. Limits are 256 documents and 128 fonts.

For each confirmed hypothesis, capture the visible before/after state, compare the underlying state vector, assign severity, and keep recommended behavior in the audit rather than the user guide's description of the shipping build.

### 9.5 Error and recovery scripts

#### ERR-01 — Document load failures

For missing, unreadable, oversized, empty, and malformed-input documents:

1. Attempt direct startup opening and in-document opening separately.
2. Record the visible title/message and whether the prior valid document remains available.
3. Press Enter, Esc, Menu, and `Ctrl+O` as applicable; verify a recovery path exists.
4. Press `Ctrl+Esc`; verify normal exit.
5. Relaunch a known-good document and verify no failed-load state leaked into its sidecar.

#### ERR-02 — Font load failures

For invalid signature, truncated, oversized, unsupported outline, and missing font files:

1. Begin with a known working file assigned to Body, Monospace, or CJK.
2. Make a previously assigned file unreadable/oversized, then apply or restore
   that path through the Font Manager or preference.
3. Verify a local error dialog, unchanged active registry, unchanged source anchor, and responsive input.
4. Close with Esc, reopen Fonts, and verify the previous working assignments remain checked.
5. Repeat for Body, Monospace, and CJK; separately verify a missing optional
   italic/bold companion falls back to synthesis without discarding its regular role.

#### ERR-03 — Link and anchor failures

1. Activate a valid internal anchor and verify direct navigation.
2. Activate a missing internal anchor and verify a dismissible `Link target not found` dialog.
3. Activate a valid relative Markdown link and verify the new document plus optional fragment.
4. Activate a missing relative Markdown link and verify the current document remains intact.
5. Activate a non-Markdown asset and external URL; verify neither attempts unsupported rendering or leaves the reader unresponsive.
6. Use a paragraph containing three links, press Enter with that paragraph at the top of the viewport, and record which link activates.
7. Verify there is no false claim of visible link selection; evaluate whether a user can discover or choose the second and third links.
8. Scroll so a different block becomes current and repeat, confirming activation follows the current block rather than a hidden stale link.

## 10. Visual and Accessibility Review

Every captured state will be reviewed at actual 320×240 size for:

- Selected-row visibility.
- Text clipping and ellipsis needs.
- Modal size and placement.
- Background distraction behind overlays.
- Header/title hierarchy.
- Page-number collision with long filenames.
- Consistent margins and panel padding.
- Contrast in all four theme/contrast combinations.
- Legibility of 9-12 px interface text.
- Whether tab state is understandable without color alone.
- Whether controls communicate their available actions.
- Whether errors explain recovery.
- Whether touch-only actions have keyboard alternatives.
- Whether state changes are communicated rather than silently applied.

The audit will not claim full WCAG compliance because the calculator platform does not expose conventional browser semantics or assistive-technology APIs.

## 11. Evidence and Verification Workflow

1. Re-run `make test` and record the complete result.
2. Build the normal Ndless and current Firebird fixtures.
3. Run:
   - `make firebird-toc-test`
   - `make firebird-browser-cancel-test`
   - `make firebird-theme-test`
   - `make firebird-keymap-test`
4. Use the existing desktop CLI for current-run screenshots. End each deterministic event prefix with the semantic `quit` event so the loop exits before its automatically appended Back events are consumed; the final presented frame therefore remains the requested open overlay. Use separate deterministic prefixes for before/change/reopen/reverse checkpoints.
5. Where a scenario needs internal state that screenshots cannot prove, create a temporary build-only Viewer driver under `build/ui-ux-audit/`, compile it against the existing desktop objects, and emit the state vector and frame after each semantic event. Do not add this temporary driver to the repository or modify production/test harness sources.
6. Use PocketJS/Firebird only for physical actions its pinned tape actually supports: `n2`, Enter, Doc, Esc, standalone Ctrl, Tab, directional/touch actions, and waits. Supported gesture tapes are `touch-center → touch-down → touch-release`, `touch-center → touch-left → touch-release`, and `touch-center → touch-right → touch-release`.
7. Do not claim Firebird verification for Ctrl chords (`Ctrl+Esc/F/T/D/O/N/B`),
   production Menu, Scratchpad, Plus/Minus, Delete, letters or numeric keys other
   than `n2`, Shift modifiers, or physical touchpad click. The current tape
   cannot express those inputs. Verify them through raw Ndless input tests and
   mark physical-device verification as hardware required.
8. Treat the existing Doc-to-Settings theme fixture as a fixture-only
   substitution because PocketJS cannot inject production Menu. The
   Doc-to-OpenMenu TOC fixture exercises the production Doc mapping; distinguish
   the physical key name from the internal `OpenMenu` semantic event.
9. Capture current-run screenshots only.
10. Inspect every screenshot at native resolution and enlarged nearest-neighbor scale.
11. Correlate observed behavior with:
   - Production Ndless key mapping.
   - Viewer event routing.
   - Application-level document-browser interception.
   - Reader-state persistence.
12. Record serial milestones and frame counts for freeze-sensitive flows. Every such flow must end with an additional navigation/repaint probe and, where an exit is expected, `EXIT_OK` plus a returned TI-OS framebuffer.
13. For every matrix cell, label evidence as Desktop semantic, Firebird physical, Source-confirmed risk, or Hardware required.
14. Keep real-hardware-only conclusions explicitly separate from simulator results.

## 12. Audit Finding Format

Each finding will use this template:

- ID and concise title.
- Severity: P0, P1, P2, or P3.
- Evidence status:
  - Confirmed in Firebird.
  - Confirmed in desktop harness.
  - Confirmed by code path.
  - Suspected; hardware verification required.
- Affected state.
- User goal.
- Reproduction steps.
- Current result.
- Expected result.
- User impact.
- Screenshot or trace.
- Recommended behavior.
- Regression scenarios for a future fix.

Severity definitions:

- P0: freeze, unrecoverable state, data loss, or failure to return control.
- P1: blocked navigation/exit, unexpected destructive action, or input leaking through a modal.
- P2: significant inconsistency, poor discoverability, confusing hierarchy, or missing feedback.
- P3: visual polish, wording, spacing, or minor efficiency issue.

## 13. Acceptance Criteria

The work is complete when:

- Every production input event is represented in the guide.
- Every settings row has its default, range, behavior, and persistence documented.
- Every settings row has an explicit forward transition, open-state checkpoint, Esc close, document navigation/liveness checkpoint, reopen checkpoint, reverse transition, second close/navigation checkpoint, boundary sweep, and restart check.
- Every overlay has documented open, navigate, activate, cancel, and exit behavior.
- Startup and in-document file-browser cancellation are distinguished.
- Nested font-menu navigation is documented.
- Scroll and Page Swipe modes are clearly differentiated.
- Search modes and calculator text-entry limits are explained.
- Simulator-only key aliases are excluded from user instructions.
- The audit contains current-run evidence for all major surfaces.
- Every accepted screenshot is native 320×240 and represents the requested checkpoint rather than only the final state of a longer script.
- Every input/state matrix cell names its evidence backend, and no PocketJS result claims support for a key or chord absent from the pinned tape.
- Every freeze-sensitive scenario ends with a liveness probe and clean exit verification where applicable.
- Background-input cases compare source anchor, page, pan, theme, font size, bookmark count, and active search match before opening and after closing the overlay.
- The user guide describes observed shipping behavior; desired changes appear only in audit recommendations.
- Every finding has severity, reproduction, impact, recommendation, and verification status.
- README no longer acts as a conflicting second source of truth.
- Desktop tests and existing Firebird regressions still pass after documentation-only changes.
- No application, harness, font, generated source, or state-format file is modified.
- The handoff links both documents and asks once whether the findings should also be plotted visually in Figma.
