# nMarkdown UI/UX Audit — 2026-07-14

## Verdict

**Release recommendation: hold the current build for UI correctness work.** The reader has a credible core—direct Markdown loading, working CJK and monospaced font roles, broad layout controls, repeatable table-of-contents navigation, simulator-confirmed startup cancellation, and deterministic 320 × 240 rendering—but it is not yet safe or predictable enough for normal calculator reading.

Five behaviors are release-blocking:

1. Inline mathematics visibly overlaps prose at supported 18 px and 22 px font sizes.
2. Several visible overlays allow page movement, reflow, bookmarking, or activation to reach the document behind them.
3. Enter on an empty retained Bookmarks tab can activate an invisible Table of Contents row.
4. A failed direct document launch silently shows the built-in demonstration document instead of the failure.
5. A restored Page Swipe position can open on an apparently empty final page.

The audit records **5 P1 findings and 11 P2 findings**. “Capture pass” in the evidence index means that the test executable and screenshot pipeline completed; it does not mean the pictured experience is acceptable. No finding in this report is claimed to be fixed.

Companion documents:

- [Executable audit plan](UI_UX_AUDIT_PLAN.md)
- [Complete screenshot evidence index](UI_UX_AUDIT_EVIDENCE_2026-07-14.md)
- [Physical-hardware blockers](UI_UX_AUDIT_BLOCKERS_2026-07-14.md)
- [User guide and control reference](USER_GUIDE.md)

## Severity and run-health definitions

| Label | Meaning |
| --- | --- |
| **P1** | Blocks reliable reading, causes an action in the wrong UI layer, makes the requested document indistinguishable from a fallback, or makes content appear lost. |
| **P2** | Materially confusing or inefficient, but a discoverable workaround usually exists and the document remains recoverable. |
| **Healthy** | The tested behavior matched the interaction contract within the stated backend boundary. |
| **Concern** | The path works, but the run exposed a usability defect, ambiguity, or incomplete proof. |
| **Broken** | A required behavior visibly failed or the interaction is unsafe. |
| **Blocked** | The available backend cannot prove the success criterion; this is not a pass or failure of the application. |

## Method and limits

The review combined source inspection with executable visual testing:

- The desktop semantic harness completed **306/306 captures** across 26 planned scenario groups, with no command or image-conversion failures.
- Thirty-two additional desktop checkpoints reproduced navigation, error, state-restoration, and inline-math issues.
- PocketJS-NSpire / Firebird produced **seven formal framebuffer passes**, one
  document-exit run with an ordered `EXIT_OK` serial pass but an inconclusive
  stable-TI-OS-frame assertion, and one documented complex-math golden-pixel
  verification failure.
- The evidence index embeds **347 native 320 × 240 screenshots**.
- Representative screenshots appear inline below; the evidence index retains every event tape and frame.

The desktop backend runs the real reader and layout code at 320 × 240, but uses semantic events. It does not prove TI key scanning, Ctrl chord detection, touchpad thresholds, TI-OS return behavior, memory pressure, or physical LCD appearance. Firebird boots the CX II simulator, transfers a real Ndless <code>.tns</code>, replays physical-key tapes where supported, and verifies ordered serial milestones and framebuffer pixels. It is still not physical-calculator evidence.

Two Firebird fixtures map <kbd>Doc</kbd> to Settings or Menu solely to make those semantic actions reachable in the simulator tape. <kbd>Doc</kbd> is not a production nMarkdown shortcut.

The complex math-review Firebird run is a **harness/golden mismatch, not evidence of an application freeze**. Its serial trace reached read, parse, render, and present, and it produced a valid final frame. The verifier timed out because 12 of 22 pinned pixel expectations did not match the current frame, so its golden coordinates and colors must be reviewed before that fixture can provide a pass/fail UX signal.

![Complex math frame produced before the golden-pixel timeout](images/ui-ux-audit-2026-07-14/FIREBIRD-MATH-REVIEW-FAIL-timeout.png)

The separate open-document Esc tape emitted `EXIT_OK` and captured TI-OS after
the app returned. Its host-side result is intentionally classified below a
formal Firebird pass because the resumed TI-OS frame did not settle into the
harness's stable-frame condition.

![TI-OS after Esc exited an open document](images/ui-ux-audit-2026-07-14/FIREBIRD-NAV-01-document-exit-serial-pass.png)

## What is already working well

- **Direct Markdown reading is real, not a package flow.** Documents open as Markdown files, and the header uses the document filename.
- **Startup cancellation is simulator-confirmed.** Esc from the startup browser reaches the ordered <code>EXIT_OK</code> trace and returns to TI-OS.
- **Repeated TOC navigation is stable in Firebird.** One process jumped forward twice, backward once, then cancelled the overlay with at least twelve presented frames; the previously reported second-jump freeze did not reproduce in this fixture.
- **Page Swipe can advance exactly one complete page.** The Firebird page fixture verified the 2/3 progress extent, top-right page counter, and beginning of page 2.
- **Body, built-in monospace, and separately loaded CJK rendering work.** Firebird reached <code>FONT_READY</code> and rendered CJK text plus monospaced code in the same frame.
- **Default math rendering is simulator-stable.** The normal math fixture passed 28 pinned pixel checks for inline/display math, radicals, fraction rules, matrices, vectors, and brace edges.
- **Settings have broad, immediate ranges.** Font size 12–22 px, automatic/manual line spacing, side margins, table mode, code mode, contrast, reading mode, and font roles all have executable coverage.
- **Most dedicated overlays isolate ordinary page keys correctly.** Search, document browser, font browser, diagnostics, and message dialogs consume their own navigation in the tested semantic paths.
- **In-document oversized-file errors preserve the current document and provide a close action.**
- **Valid relative documents, valid anchors, missing targets, external links, and asset links all receive an explicit navigation or dialog outcome.**
- **Per-document settings are encoded with signature, version, identity, and checksum checks; valid A/B setting restoration worked in the desktop study.**

![Firebird CJK and monospaced-code evidence](images/ui-ux-audit-2026-07-14/FIREBIRD-FORMATS-01-cjk-code.png)

![Firebird repeated TOC jump evidence](images/ui-ux-audit-2026-07-14/FIREBIRD-NAV-06-toc-repeated-jumps.png)

## Prioritized findings

### 1. P1 — Inline math overlaps ordinary prose at supported font sizes

**Affected plan steps:** [SET-02 — every font size](UI_UX_AUDIT_PLAN.md#set-02--font-size-every-value-from-12-px-through-22-px), Reading and layout, Visual and accessibility review.

**Exact reproduction A (18 px):**

1. Launch <code>samples/math-gallery.md</code> in fresh Light/15 px state.
2. Press <kbd>+</kbd> three times to reach 18 px.
3. Open Search with <kbd>Ctrl</kbd>+<kbd>F</kbd>.
4. Type <code>Inline baseline</code>.
5. Press <kbd>Enter</kbd>.

**Exact reproduction B (22 px):**

1. Repeat from fresh Light/15 px state.
2. Press <kbd>+</kbd> seven times to reach 22 px.
3. Search for <code>Inline baseline</code> and activate the result.

**Expected:** An inline formula behaves as a baseline-aligned inline replaced element. Its ascent and descent expand the containing line box, adjacent prose starts after its measured advance, and the next line begins below the formula’s full ink bounds.

**Current:** At 18 px and again at 22 px, scripts, fractions, and formula ink collide with surrounding or following prose. The 22 px frame makes part of the second line unreadable. This occurs inside the supported user-facing size range, so it cannot be treated as an extreme-input artifact.

**Recommendation:** Derive each line’s ascent, descent, and leading from the maximum text-or-math bounds on that line; align math to the prose baseline using the math box’s declared baseline; include italic correction and actual horizontal advance; and wrap before committing a run whose full advance exceeds the remaining width. Add image assertions at every size 12–22 px in both themes, with mixed fractions, roots, scripts, delimiters, CJK, and trailing punctuation.

![18 px inline-math collision](images/ui-ux-audit-2026-07-14/INLINE-MATH-threshold-18.png)

![22 px inline-math collision](images/ui-ux-audit-2026-07-14/INLINE-MATH-03-valid-22-light.png)

### 2. P1 — Visible overlays do not consistently own all input

**Affected plan steps:** [Canonical modal input isolation](UI_UX_AUDIT_PLAN.md#modal-input-isolation), [INPUT-01](UI_UX_AUDIT_PLAN.md#input-01--overlay-input-isolation), [INPUT-02](UI_UX_AUDIT_PLAN.md#input-02--key-repeat-and-simultaneous-modifier-priority), SET-02, SET-08.

**Exact reproductions:**

1. Open a long document, move to a recognizable reading position, open Settings, then press <kbd>Page Down</kbd>. Close Settings and compare the document position.
2. Open the TOC over a long document, issue a pointer-scroll gesture, then close the TOC.
3. Open the generic Reader controls overlay in a document with no headings or bookmarks, then press <kbd>Page Down</kbd>.
4. With Settings, TOC, or Reader controls visible, press <kbd>+</kbd>/<kbd>-</kbd>, <kbd>Ctrl</kbd>+<kbd>B</kbd>, or <kbd>Ctrl</kbd>+<kbd>N</kbd> and inspect the document/state after closing.

**Expected:** A visible modal layer consumes every non-global event. Only that layer’s documented navigation, close action, and immediate global exit may execute. Background scrolling, paging, reflow, bookmark mutation, search-next, link activation, and theme changes must be impossible.

**Current:** Settings, TOC, and the generic Reader controls share the fallback event path. Page and pointer events can move the document; global-looking size/bookmark/search actions can mutate hidden state; and generic Enter can act on the document. Dedicated browser/search/diagnostic/message paths are better isolated, which makes the inconsistency harder for a user to predict.

**Recommendation:** Introduce one ordered modal dispatcher with an explicit “consumed” result. Resolve immediate quit first, then the single topmost overlay, then focused wide content, and only then document commands. Each overlay should have an allow-list; unknown events should be consumed as no-ops rather than falling through.

![Page movement leaked through Settings](images/ui-ux-audit-2026-07-14/INPUT-01-settings-page-after.png)

![Pointer movement leaked through TOC](images/ui-ux-audit-2026-07-14/INPUT-01-toc-scroll-after.png)

![Page movement leaked through generic Reader controls](images/ui-ux-audit-2026-07-14/INPUT-01-generic-page-after.png)

### 3. P1 — Empty Bookmarks can activate an invisible TOC selection

**Affected plan steps:** [NAV-07 — Bookmarks](UI_UX_AUDIT_PLAN.md#nav-07--bookmarks-add-navigate-remove-and-persist), [NAV-06 — TOC](UI_UX_AUDIT_PLAN.md#nav-06--table-of-contents-repeated-and-reverse-jumps), Menus and lists.

**Exact reproduction:**

1. Open <code>interaction-gallery.md</code>.
2. Open Menu/TOC.
3. Press <kbd>Down</kbd> five times so Epsilon is the retained TOC selection.
4. Press <kbd>Esc</kbd>.
5. Add a bookmark with <kbd>Ctrl</kbd>+<kbd>B</kbd>.
6. Open Menu and press <kbd>Right</kbd> to select Bookmarks.
7. Press <kbd>Esc</kbd>, then remove the only bookmark with <kbd>Ctrl</kbd>+<kbd>B</kbd>.
8. Open Menu again. It retains the now-empty Bookmarks tab.
9. Press <kbd>Enter</kbd>.

**Expected:** The empty Bookmarks view says that no bookmarks exist, has no selected row, and Enter is a consumed no-op.

**Current:** The overlay appears empty, but Enter uses the retained TOC index and jumps to Epsilon. The operation has no visible target and violates the active tab’s meaning.

**Recommendation:** Model the active list and selection as one state, clear/clamp selection whenever the tab or collection changes, and route activation exclusively through the active list. Render a true empty-state message such as “No bookmarks yet — Ctrl+B adds one”; disable activation until a row exists.

![Invisible Enter activation after the empty Bookmarks view](images/ui-ux-audit-2026-07-14/NAV-07-empty-bookmarks-enter-result.png)

### 4. P1 — Failed direct document launch silently displays the built-in document

**Affected plan steps:** [ERR-01 — Document load failures](UI_UX_AUDIT_PLAN.md#err-01--document-load-failures), Error recovery, Startup document browser.

**Exact reproduction:**

1. Launch nMarkdown directly with the unreadable fixture <code>04-unreadable.md</code> as the document argument.
2. Provide no input.
3. Inspect the first frame.
4. Repeat with missing, oversized, or malformed direct-launch fixtures.

**Expected:** The filename and failure reason are visible, the requested document is not represented as successfully open, and the screen offers explicit recovery: choose another Markdown file, retry, or exit.

**Current:** The app shows the built-in nMarkdown Unicode demonstration page with a red progress treatment but no visible error message, failed filename, or recovery explanation. A user can reasonably believe the wrong file was opened successfully. Source inspection shows a stored document error string, but this direct-error surface does not render it.

**Recommendation:** Replace the fallback document with a dedicated startup error state. Show a short title, basename, reason, and key actions; keep the long path/details behind a secondary view. Never reuse ordinary document chrome for an unsuccessful direct open.

![Unreadable direct launch silently falling back to the built-in page](images/ui-ux-audit-2026-07-14/ERR-01-direct-unreadable.png)

### 5. P1 — Restored Page Swipe state can open on a blank final page

**Affected plan steps:** [SET-08 — Reading mode](UI_UX_AUDIT_PLAN.md#set-08--reading-mode-scroll-and-page-swipe), [STATE-01](UI_UX_AUDIT_PLAN.md#state-01--per-document-state-versus-session-state), [STATE-02](UI_UX_AUDIT_PLAN.md#state-02--individual-ab-persistence-cycle-for-every-persisted-setting).

**Exact reproduction:**

1. Configure document A for Dark, 18 px, +6 px line gap, 10 px margins, Grid tables, Wrap code, High contrast, and Page Swipe.
2. Advance in A and store a bookmark/position.
3. Open and configure document B with the opposing settings.
4. Reopen A to verify its sidecar.
5. Restart A in a fresh process without input.

**Expected:** The nearest content-bearing page is restored, the reading anchor remains recognizable, and the page counter corresponds to visible content.

**Current:** A restarts on a black, apparently empty <code>3 / 3</code> page. Even if the persisted numeric endpoint is technically within the document range, the first surface suggests content loss and gives no clue whether moving backward is required.

**Recommendation:** Persist a semantic source anchor plus intra-block offset, rebuild pages, resolve to the nearest valid content unit, and reject pages with no visible content except deliberate blank blocks. Clamp the final page start to the last content-bearing page and add a restart assertion for nonempty visible pixels below chrome.

![Restored Page Swipe state on blank 3/3](images/ui-ux-audit-2026-07-14/STATE-01-reader-restarted.png)

### 6. P2 — Esc dismisses hidden wide focus before the visible Settings overlay

**Affected plan steps:** [Escape and exit contract](UI_UX_AUDIT_PLAN.md#escape-and-exit), [NAV-01](UI_UX_AUDIT_PLAN.md#nav-01--esc-back-stack-from-every-state), NAV-10.

**Exact reproduction:**

1. Search <code>interaction-gallery.md</code> for <code>codeprobe</code>.
2. Press <kbd>Enter</kbd> to activate the result.
3. Press <kbd>Enter</kbd> again to focus the wide code block.
4. Open Settings with <kbd>Ctrl</kbd>+<kbd>T</kbd>.
5. Press <kbd>Esc</kbd> once.

**Expected:** One Esc closes the visible topmost Settings panel. The underlying wide-focus state may remain or be cleared consistently, but it must not consume a key ahead of a visible modal.

**Current:** The first Esc clears the hidden code focus and leaves Settings visible. A second Esc is required to close the panel. This can be mistaken for a freeze or ignored key.

**Recommendation:** Define z-order explicitly: immediate quit, message/nested browser, primary overlay, wide-content focus, document, application. Back should always act on the highest visible layer first.

![Settings still visible after the first Esc](images/ui-ux-audit-2026-07-14/NAV-01-wide-focus-settings-after-first-esc.png)

### 7. P2 — Generic Reader controls are simultaneously help, menu, and hidden document command surface

**Affected plan steps:** [NAV-08 — Generic controls](UI_UX_AUDIT_PLAN.md#nav-08--generic-controls-overlay-for-a-document-without-headings), INPUT-01, Canonical interaction contract.

**Exact reproduction:**

1. Open <code>no-headings.md</code>, which has no TOC rows or bookmarks.
2. Press Menu to open Reader controls.
3. Press Menu again.
4. Reopen it and press <kbd>Enter</kbd>.
5. Reopen it, press <kbd>Page Down</kbd>, close it, and compare the reading position.

**Expected:** Menu toggles the visible controls panel; Enter either closes it or does nothing; page keys do not move hidden content. The overlay describes actual controls without executing an undocumented action.

**Current:** Menu does not close the generic panel, Enter toggles the underlying light/dark theme, and Page Down moves the hidden document. The displayed line “Enter: light / dark” turns a help screen into an action surface without a focused control.

**Recommendation:** Make this either a passive Help dialog or a real selectable command menu, not both. For passive Help, Esc/Menu close and every other event is consumed. For a command menu, render focusable rows and activate only the selected row.

![Generic Reader controls](images/ui-ux-audit-2026-07-14/NAV-08-controls-open.png)

![Menu pressed while the generic panel remains open](images/ui-ux-audit-2026-07-14/NAV-08-menu-self-result.png)

![Enter changed the underlying theme](images/ui-ux-audit-2026-07-14/NAV-08-enter-result.png)

### 8. P2 — Contrast changes immediately, but its open-panel label remains stale

**Affected plan steps:** [SET-07 — Standard/High in both themes](UI_UX_AUDIT_PLAN.md#set-07--contrast-standard-and-high-in-both-light-and-dark-themes), Settings documentation, Visual review.

**Exact reproduction:**

1. Open Settings.
2. Move to Contrast.
3. Press <kbd>Right</kbd> to switch Standard to High.
4. Keep Settings open and inspect the selected row.
5. Close and reopen Settings.
6. Repeat High to Standard in both Light and Dark.

**Expected:** Palette and label update in the same frame, so the selected row always names the active value.

**Current:** The palette changes, but the open panel continues to say “Contrast: Standard” until Settings is rebuilt by closing/reopening. A visually impaired user is asked to infer the setting from the very palette they are trying to configure.

**Recommendation:** Rebuild or update the Contrast glyph run in the setting mutator, just as Theme already does. Add a same-frame assertion for both directions and both themes.

![High-contrast palette with stale Standard label](images/ui-ux-audit-2026-07-14/SET-07-dark-high-settings.png)

### 9. P2 — Oversized font files report a false missing-file error

**Affected plan steps:** [ERR-02 — Font load failures](UI_UX_AUDIT_PLAN.md#err-02--font-load-failures), [SET-09 — Fonts](UI_UX_AUDIT_PLAN.md#set-09--fonts-body-monospace-and-cjk), Error recovery.

**Exact reproduction:**

1. Open Settings → Fonts.
2. Choose Body, Monospace, or CJK.
3. Select <code>02-oversized.ttf</code>.
4. Read the dialog.
5. Repeat for all three roles.

**Expected:** The dialog says that the font exceeds the supported size limit, identifies the selected role/file, preserves the current font, and returns to the same list position.

**Current:** Every role reports “could not open document: No such file or directory.” The initial size-limit failure is overwritten by the automatic <code>.tns</code> retry, so the user is told to solve the wrong problem.

**Recommendation:** Preserve the most specific first failure and retry only when the original path truly does not exist. Use font-specific wording throughout; include actual and maximum size where space permits.

![Oversized Body font with misleading missing-file message](images/ui-ux-audit-2026-07-14/ERR-02-body-oversized.png)

### 10. P2 — Search exposes raw Markdown and clips the context needed to choose a result

**Affected plan steps:** [SRCH-01](UI_UX_AUDIT_PLAN.md#srch-01--search-entry-modes-results-closereopen-and-next), Search input, Visual and accessibility review.

**Exact reproduction:**

1. Open Search in <code>interaction-gallery.md</code>.
2. Search for <code>comparison</code>.
3. Page through the result list.
4. Search for text near headings, links, and tables.
5. Enter a query at the 64-byte boundary.

**Expected:** Results show readable rendered-text context, a stable location cue such as heading name, and an ellipsis when context is omitted. The selected match stays visible, and long query text has a cursor/scroll treatment or an explicit limit.

**Current:** Snippets come from raw source and expose heading markers, table pipes/separators, and link syntax. Several lines begin mid-word and run beyond the panel without ellipses. Long query text is clipped at a byte limit with no visible remaining-capacity or horizontal navigation.

**Recommendation:** Build a source-to-visible-text search index that retains source offsets for navigation. Generate grapheme-safe, word-boundary snippets around the match, strip Markdown syntax, prefix the containing heading, and ellipsize both sides. Count input by Unicode scalar/grapheme or clearly label the byte limit.

![Raw Markdown markers and clipped search snippets](images/ui-ux-audit-2026-07-14/SRCH-01-results.png)

![Clipped maximum-length query](images/ui-ux-audit-2026-07-14/SRCH-01-input-limit.png)

### 11. P2 — The document browser is flat, ambiguous, and silently capped

**Affected plan steps:** [NAV-04 — Startup browser](UI_UX_AUDIT_PLAN.md#nav-04--startup-document-browser), [NAV-05 — In-document browser](UI_UX_AUDIT_PLAN.md#nav-05--in-document-document-browser), Menus and lists.

**Exact reproduction:**

1. Start in a root containing two Markdown files with the same basename in different folders.
2. Inspect their browser rows.
3. Start in the cap fixture containing more than the supported result count.
4. Page to the final browser window.

**Expected:** Duplicate names are disambiguated with a compact relative parent path; folders or breadcrumbs reveal hierarchy; and truncation is explicit with a count or “more files not shown” row.

**Current:** Rows render only basenames, so duplicate files are visually identical. Recursive enumeration stops at 256 documents and depth 12, but the UI gives no truncation notice. Font enumeration similarly stops at 128. The result is deterministic but not discoverable.

**Recommendation:** Show basename plus the shortest unique relative parent, add a header with current root and count, and render a final warning row when enumeration reaches any depth/result limit. A compact folder-first browser would reduce both ambiguity and scanning cost.

![Two indistinguishable duplicate basenames](images/ui-ux-audit-2026-07-14/NAV-04-duplicate-basenames.png)

![Final window of a capped browser list](images/ui-ux-audit-2026-07-14/NAV-04-cap-last-window.png)

### 12. P2 — Wrapped code remains focusable as if horizontal panning were available

**Affected plan steps:** [SET-06 — Code Pan/Wrap](UI_UX_AUDIT_PLAN.md#set-06--code-blocks-pan-and-wrap), [NAV-10 — Wide focus](UI_UX_AUDIT_PLAN.md#nav-10--wide-code-table-and-formula-focus), Reading and layout.

**Exact reproduction:**

1. Open the interaction gallery and set Code blocks to Wrap.
2. Navigate to <code>codeprobe</code>.
3. Press <kbd>Enter</kbd>.
4. Press left/right and then Esc.

**Expected:** A code block in Wrap mode is not treated as horizontally wide unless its actual laid-out line still overflows. Enter should follow the normal link/theme contract or be a no-op; no wide-focus outline/pan state should appear.

**Current:** Any code background is classified as wide. Enter therefore draws the wide-focus state even when wrapping has removed horizontal overflow. The focus suggests a panning affordance that does not correspond to the configured mode.

**Recommendation:** Determine focusability from post-layout overflow, not block kind. Expose a visible focus affordance only when at least one line has positive horizontal overflow and the active mode permits pan.

![Wrapped code still receiving wide focus](images/ui-ux-audit-2026-07-14/SET-06-wrap-focus-result.png)

### 13. P2 — Overlay shortcuts do not follow one predictable switching rule

**Affected plan steps:** [NAV-03 — Shortcut switching](UI_UX_AUDIT_PLAN.md#nav-03--shortcut-switching-between-overlays), Escape and exit, Modal input isolation.

**Exact reproduction:**

1. Open Search, then try Settings, Menu, Diagnostics, and Open Document shortcuts one at a time.
2. Repeat from Settings, TOC, Diagnostics, document browser, font role list, and font file list.
3. Observe whether the current overlay closes, switches, ignores the shortcut, or opens a nested state.

**Expected:** The same shortcut has the same policy everywhere: either switch directly to its target overlay, toggle itself closed, or be deliberately unavailable with feedback. Nested font Back behavior can remain special, but top-level shortcuts should be uniform.

**Current:** Search ignores several switch requests; settings and TOC switch for some shortcuts; diagnostics and font lists follow other rules; Open Document is intercepted differently; and the browser’s self-shortcut behavior differs from the main overlays. Users must memorize a state-by-state matrix.

**Recommendation:** Publish and implement one transition table. A simple policy is: Ctrl+Esc exits; Esc closes one visible layer; invoking the active overlay shortcut closes it; invoking another primary overlay replaces the current primary overlay; nested pickers consume primary shortcuts until cancelled.

![Search after a Settings switch attempt](images/ui-ux-audit-2026-07-14/NAV-03-search-to-settings.png)

![Font role menu after a Settings switch attempt](images/ui-ux-audit-2026-07-14/NAV-03-font-roles-to-settings.png)

### 14. P2 — Enter always activates the first link in the current block

**Affected plan steps:** [ERR-03 — Link and anchor failures](UI_UX_AUDIT_PLAN.md#err-03--link-and-anchor-failures), [NAV-09](UI_UX_AUDIT_PLAN.md#nav-09--diagnostics-and-informationalerror-dialogs), Global controls.

**Exact reproduction:**

1. Navigate to the “Multiple Links action” paragraph containing First, Second, and Third links.
2. Press <kbd>Enter</kbd>.
3. Return and try to choose the second or third link using available navigation keys.

**Expected:** A visible link focus/cursor identifies the target, or Enter opens a compact chooser listing all links in the current block.

**Current:** There is no per-link focus. Enter scans layout runs and activates the first link, so later links in the same block are keyboard-inaccessible.

**Recommendation:** Add link navigation with previous/next focus and a strong non-color-only indicator, or show a numbered link chooser when a block contains multiple links. Preserve the active link across dialog close.

![Three visible links with no way to choose beyond the first](images/ui-ux-audit-2026-07-14/ERR-03-multiple-links.png)

### 15. P2 — State rejection and persistence failures are silent

**Affected plan steps:** [STATE-03 — Sidecar rejection](UI_UX_AUDIT_PLAN.md#state-03--sidecar-rejection-silent-fallback-and-save-timing), Persistence and font behavior, Error recovery.

**Exact reproduction:**

1. Launch with valid dark state and confirm it loads.
2. Repeat with corrupted signature, unsupported version, mismatched checksum, truncated bytes, and stale document identity.
3. On a real calculator, repeat where the document is readable but the adjacent state file is not writable.

**Expected:** Corrupt/stale state may safely fall back to defaults, but the user receives a concise one-time notice when saved position/settings could not be restored or saved. The notice distinguishes corruption, stale document identity, and storage permission.

**Current:** Desktop fallback is safe but silent. A reader sees changed appearance/position with no explanation. Read/write failures are ignored at the application boundary, so a user can repeatedly configure a document without knowing persistence is unavailable.

**Recommendation:** Keep safe fallback, add a nonmodal status banner or Diagnostics entry, and expose “state not saved” after a write failure. Deduplicate warnings per session. Physical permission behavior remains unverified.

![Checksum rejection falling back without user feedback](images/ui-ux-audit-2026-07-14/STATE-03-checksum-fallback.png)

### 16. P2 — Empty, binary-like, and unreadable documents have inconsistent recovery semantics

**Affected plan steps:** [ERR-01 — Document load failures](UI_UX_AUDIT_PLAN.md#err-01--document-load-failures), Error recovery, Visual review.

**Exact reproduction:**

1. From a valid open document, open <code>01-empty.md</code>.
2. Reopen the valid document and open <code>03-malformed.md</code>, whose bytes begin with the OpenType <code>OTTO</code> signature.
3. Reopen the valid document and open <code>04-unreadable.md</code>.
4. Compare whether the old document is preserved and whether the reason is accurate.

**Expected:** Empty Markdown receives an intentional empty-document state; obviously binary input is rejected or explicitly shown as unsupported; unreadable files preserve the old document and report the permission failure accurately.

**Current:** Empty input replaces the document with an almost blank screen, binary-like <code>OTTO</code> bytes are rendered as ordinary Markdown, and the desktop unreadable fixture reports “No such file or directory.” Oversized input, by contrast, gives a useful dialog and preserves the previous document.

**Recommendation:** Define a single open transaction: probe/read/validate/parse/layout first, then commit only on success. Treat empty as a valid but explicit empty state, detect NUL/control-heavy or known binary signatures, and retain the original OS error category before path retries.

![Empty document with no empty-state explanation](images/ui-ux-audit-2026-07-14/ERR-01-in-document-empty.png)

![OpenType signature accepted as Markdown](images/ui-ux-audit-2026-07-14/ERR-01-in-document-malformed.png)

## Low-resolution and accessibility risks

These are design risks identified from the 320 × 240 frames; they are not claims of WCAG conformance or nonconformance. Physical LCD review is still required.

1. **Small secondary text:** Search status, diagnostics, dialog hints, and some settings rows are rendered around 9–10 px. Anti-aliasing helps in the desktop captures, but legibility on the calculator LCD has not been measured.
2. **Color-dependent focus:** Many selections are conveyed primarily by blue/cyan fill and white text. Wide-content focus adds a thin border/scroll marker, but several states lack an additional shape, cursor, checkmark, or index.
3. **Truncated critical text:** Long error messages, paths, query strings, filenames, and search snippets clip without wrapping or ellipses. The reader may lose the differentiating part of an error or path.
4. **Missing-glyph presentation:** Without an optional CJK font, affected text renders as repeated replacement diamonds. The font menu works, but the document surface does not explain that a CJK font is missing or offer a direct path to the role picker.
5. **Ambiguous empty states:** Empty documents and empty Bookmarks surfaces reuse ordinary chrome and headings instead of explaining the empty condition and available action.
6. **Focus/overlay hierarchy is not visually explicit:** Background content remains visually active under overlays, which makes leaked input especially surprising. A subtle dim layer would reinforce modality if contrast remains adequate.
7. **Long title/page competition:** The top bar is compact and the page number correctly occupies the upper right in Page Swipe, but long filenames truncate with little indication. The filename and page label need independent reserved widths and ellipsis.
8. **Touch discoverability:** Horizontal swipes provide page movement, but no in-app hint explains the gesture after switching modes. Touchpad thresholds and accidental-scroll resistance are not yet device-tested.
9. **No assistive semantic layer is expected on Ndless:** Consequently, keyboard order, visible focus, concise labels, and error recovery are the entire accessibility surface; inconsistent focus behavior carries more weight than on a platform with a screen reader.

## Source-grounded interaction risks

Source inspection supports the reproduced behavior and identifies the safest correction boundaries:

- [Viewer event routing](../src/app/viewer.cpp) contains dedicated early-return handlers for several overlays, followed by a shared generic switch for Settings, TOC, controls, wide focus, and document actions. Consolidating modality here addresses Findings 2, 3, 6, 7, 12, and 13 together.
- [Viewer search and presentation](../src/app/viewer.cpp) shapes the query and raw match snippets directly; [document search](../src/document/search.cpp) searches source bytes. A visible-text index should preserve source offsets while changing presentation.
- [File enumeration](../src/platform/stdio_files.cpp) enforces recursion/result limits; [application orchestration](../src/app/application.cpp) requests 256 document and 128 font results; [viewer browser rows](../src/app/viewer.cpp) reduce each path to a basename.
- [Reader state codec](../src/document/state.cpp) correctly validates signatures, versions, document identity, and checksums. The UX gap is reporting and write-status propagation, not absence of validation.
- [Block layout](../src/layout/block_layout.cpp) and [math layout](../src/math/math_layout.cpp) are the appropriate boundary for formula baseline/line-box repair; compensating only in drawing would preserve incorrect wrapping and page measurement.

## Physical-hardware completion blockers

The audit is not complete for release until these named checks run on a real CX/CX II. Each blocker includes an exact procedure and required screenshot/log artifact:

- [Physical Ctrl+T and +/- setting cycles](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-set-physical-ctrl-t-plus-minus)
- [Ctrl+Esc immediate exit from every state](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-nav-02-physical-ctrl-esc)
- [Physical search text entry and editing](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-srch-01-physical-entry)
- [Full physical overlay input-isolation matrix](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-input-01-physical-overlay-matrix)
- [Key repeat, chord priority, and touch timing](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-input-02-timing-and-chords)
- [Calculator sidecar read/write permissions](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-state-03-calculator-permissions)
- [Unsupported-outline and missing-after-list font fixtures](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-err-02-font-fixtures)

## Recommended implementation order

1. Correct event ownership and Back z-order in one modal dispatcher; cover generic controls and empty Bookmarks in the same change.
2. Correct math line metrics and wrapping across all supported sizes before further glyph-shape polish.
3. Replace silent startup fallback and blank restored pages with explicit, content-bearing states.
4. Repair high-signal error text: font size limit, unreadable-file category, state save/restore status.
5. Make search snippets, file paths, links, and empty states navigable and unambiguous at 320 × 240.
6. Run the physical blocker matrix, then update golden screenshots only after the intended behavior is accepted.

## Exit criteria for the next audit

The next dated audit should not change a Broken row to Healthy until:

1. The exact reproduction is rerun from a fresh fixture.
2. A native 320 × 240 before/after screenshot is retained.
3. Modal paths assert both visible overlay state and unchanged background position/settings.
4. Math tests inspect full line bounding boxes at every supported font size, not only a few golden pixels.
5. Startup and persistence tests assert visible explanatory text, not only process liveness.
6. The seven physical blocker procedures have calculator screenshots/logs.
7. The complex math golden is intentionally reviewed and repinned only after the desired renderer output is accepted.

## Canonical 39-run execution matrix

This matrix follows the 39 practical run groups in the executable plan, in
their original order. Health rates the UX shown by the run, not merely process
completion. Every row has a native screenshot, a named blocker, or both; exact
uncompressed event tapes and sibling frames are in the [evidence index](UI_UX_AUDIT_EVIDENCE_2026-07-14.md).

| # | Planned run | Health | Outcome and evidence |
| ---: | --- | --- | --- |
| 1 | SET-01 — Light/Dark theme | **Healthy** | Dark → close → scroll → reopen → Light → close → scroll repainted and retained the active value. [Dark reopened](images/ui-ux-audit-2026-07-14/SET-01-04-dark-reopened.png) |
| 2 | SET-02 — font size 12–22 px | **Broken** | The control traverses and clamps the complete range, but supported 18 px and 22 px make inline math overlap prose. [18 px collision](images/ui-ux-audit-2026-07-14/INLINE-MATH-threshold-18.png); [22 px collision](images/ui-ux-audit-2026-07-14/INLINE-MATH-03-valid-22-light.png) |
| 3 | SET-03 — Auto and +2…+10 line spacing | **Healthy** | Every value, reverse cycle, close/scroll/reopen checkpoint, and boundary clamp completed with visible reflow. [Auto restored](images/ui-ux-audit-2026-07-14/SET-03-auto-reverse-reopened.png) |
| 4 | SET-04 — 2…18 px side margins | **Healthy** | The complete range, reverse cycle, close/scroll/reopen checkpoints, and clamps changed the usable width as expected. [18 px document](images/ui-ux-audit-2026-07-14/SET-04-18-document.png) |
| 5 | SET-05 — Responsive/Grid tables | **Healthy** | Responsive records and Grid + pan are visibly distinct; Grid focus and horizontal pan work. [Grid panned](images/ui-ux-audit-2026-07-14/SET-05-grid-panned.png) |
| 6 | SET-06 — Pan/Wrap code | **Concern** | Wrapping works, but wrapped code still accepts a misleading wide-focus state. [Wrap focus](images/ui-ux-audit-2026-07-14/SET-06-wrap-focus-result.png) |
| 7 | SET-07 — Standard/High contrast | **Concern** | Both palettes apply in Light and Dark, but the open row label remains stale. [Dark/High with stale label](images/ui-ux-audit-2026-07-14/SET-07-dark-high-settings.png) |
| 8 | SET-08 — Scroll/Page Swipe | **Concern** | Ordinary mode switching and paging work; a persisted Page Swipe position can later reopen on a blank final page. [Page navigation](images/ui-ux-audit-2026-07-14/SET-08-page-right.png); [blank restored 3/3](images/ui-ux-audit-2026-07-14/STATE-01-reader-restarted.png) |
| 9 | SET-09 — Fonts menu hierarchy/cancellation | **Healthy** | Settings → role list → file list and both Esc levels are visible and navigable. [Back to roles](images/ui-ux-audit-2026-07-14/SET-09-back-to-roles.png) |
| 10 | SET-09 — Body/Monospace/CJK selection and session | **Concern** | Valid roles reflow and remain active across a document switch, but physical selection and complete CJK coverage still need hardware confirmation. [CJK document](images/ui-ux-audit-2026-07-14/SET-09-cjk-document.png); [settings-key blocker](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-set-physical-ctrl-t-plus-minus) |
| 11 | SET-09 — multi-role/font failures | **Broken** | Invalid/truncated files roll back, but oversized fonts report a false missing-file error; two failure fixtures remain blocked. [Oversized Body error](images/ui-ux-audit-2026-07-14/ERR-02-body-oversized.png); [fixture blocker](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-err-02-font-fixtures) |
| 12 | NAV-01 — semantic Esc stack | **Concern** | Most visible layers close, but hidden wide focus can consume the first Esc before visible Settings. [Settings after first Esc](images/ui-ux-audit-2026-07-14/NAV-01-wide-focus-settings-after-first-esc.png) |
| 13 | NAV-01 — simulator physical exits | **Concern** | Startup Esc is a formal Firebird pass; open-document Esc emits EXIT_OK and shows TI-OS, but its stable-TI-OS-frame assertion is inconclusive and the full state set is not physical-hardware certified. [Startup exit](images/ui-ux-audit-2026-07-14/FIREBIRD-NAV-04-startup-cancel-ti-os.png); [document exit](images/ui-ux-audit-2026-07-14/FIREBIRD-NAV-01-document-exit-serial-pass.png) |
| 14 | NAV-02 — semantic Ctrl+Esc matrix | **Healthy** | The immediate semantic Quit path terminates from all 14 modeled states and follows the final state-save path. [Settings before quit](images/ui-ux-audit-2026-07-14/NAV-02-settings-before-ctrl-esc.png) |
| 15 | NAV-02 — physical Ctrl+Esc matrix | **Blocked** | PocketJS cannot express the chord and no real-calculator artifact is available. [Hardware blocker](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-nav-02-physical-ctrl-esc) |
| 16 | NAV-03 — overlay shortcut switching | **Concern** | Shortcuts switch, close, ignore, or rebuild depending on the source overlay; there is no uniform rule. [Search → Settings attempt](images/ui-ux-audit-2026-07-14/NAV-03-search-to-settings.png) |
| 17 | NAV-04 — desktop startup browser | **Concern** | Navigation/open/cancel work, but duplicate basenames and the 256-file cap are not explained. [Duplicate basenames](images/ui-ux-audit-2026-07-14/NAV-04-duplicate-basenames.png); [cap window](images/ui-ux-audit-2026-07-14/NAV-04-cap-last-window.png) |
| 18 | NAV-04 — Firebird startup cancel | **Healthy** | Esc reaches EXIT_OK and a stable resumed TI-OS frame. [Firebird startup cancel](images/ui-ux-audit-2026-07-14/FIREBIRD-NAV-04-startup-cancel-ti-os.png) |
| 19 | NAV-05 — in-document browser | **Healthy** | Open, cancel, switch, restore, and repeated-open paths retain a readable document. [Repeated open](images/ui-ux-audit-2026-07-14/NAV-05-repeated-open-result.png) |
| 20 | NAV-06 — repeated/reverse TOC jumps | **Healthy** | Forward, second forward, backward, page navigation, and cancel remain live in desktop and Firebird. [Firebird repeated jumps](images/ui-ux-audit-2026-07-14/FIREBIRD-NAV-06-toc-repeated-jumps.png) |
| 21 | NAV-07 — bookmark lifecycle | **Broken** | Add/navigate/remove works normally, but an empty retained Bookmarks tab lets Enter activate an invisible TOC row. [Invisible jump result](images/ui-ux-audit-2026-07-14/NAV-07-empty-bookmarks-enter-result.png) |
| 22 | SRCH-01 — semantic search | **Concern** | Four modes, bounds, activation, reopen, next, and document switch work; snippets expose raw Markdown and clip context. [Search results](images/ui-ux-audit-2026-07-14/SRCH-01-results.png) |
| 23 | SRCH-01 — physical text entry | **Blocked** | PocketJS cannot type the required calculator keys and no hardware capture exists. [Hardware blocker](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-srch-01-physical-entry) |
| 24 | NAV-08 — generic Reader controls | **Broken** | Menu does not toggle it closed, Enter changes the document theme, and page movement leaks behind it. [Enter result](images/ui-ux-audit-2026-07-14/NAV-08-enter-result.png) |
| 25 | NAV-09 — Diagnostics | **Healthy** | Open, refresh, close, self-shortcut, and Menu outcomes remain readable and live. [Refreshed diagnostics](images/ui-ux-audit-2026-07-14/NAV-09-diagnostics-refreshed.png) |
| 26 | NAV-09 — dialogs | **Healthy** | External, missing-anchor, failed-relative, asset, font, and document dialogs produce explicit stable outcomes. [Missing relative link](images/ui-ux-audit-2026-07-14/NAV-09-dialog-missing-relative.png) |
| 27 | NAV-10 — wide code/table/formula focus | **Concern** | Panning works, but focus eligibility is block-kind based and includes wrapped code with no useful overflow. [Code focused](images/ui-ux-audit-2026-07-14/NAV-10-code-focused.png) |
| 28 | INPUT-01 — semantic overlay isolation | **Broken** | Settings, TOC/Bookmarks, and generic controls leak movement or hidden commands into the document. [Settings leak](images/ui-ux-audit-2026-07-14/INPUT-01-settings-page-after.png) |
| 29 | INPUT-01 — simulator-supported gestures | **Blocked** | The planned overlay gesture tapes were not completed as formal Firebird passes; Page Swipe itself has simulator evidence, but it does not prove overlay isolation. [Page fixture](images/ui-ux-audit-2026-07-14/FIREBIRD-PAGE-01-page-2.png); [isolation blocker](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-input-01-physical-overlay-matrix) |
| 30 | INPUT-01 — full physical overlay matrix | **Blocked** | Required Menu, Plus/Minus, Ctrl shortcuts, and touch combinations need a real calculator. [Hardware blocker](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-input-01-physical-overlay-matrix) |
| 31 | INPUT-02 — semantic priority/routing | **Concern** | Semantic repeatable/non-repeatable routing and quit priority execute, but semantics cannot prove timing or simultaneous modifiers. [Modifier contexts](images/ui-ux-audit-2026-07-14/INPUT-02-modifier-contexts.png) |
| 32 | INPUT-02 — physical timing/chords | **Blocked** | Hold delay, repeat interval, simultaneous modifiers, and keyboard-versus-touch arbitration remain device-only. [Hardware blocker](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-input-02-timing-and-chords) |
| 33 | STATE-01 — combined A/B state | **Broken** | Distinct A/B settings restore, but A can restart on an apparently empty 3/3 page. [A restored settings](images/ui-ux-audit-2026-07-14/STATE-01-reader-restored.png); [blank restart](images/ui-ux-audit-2026-07-14/STATE-01-reader-restarted.png) |
| 34 | STATE-02 — per-setting A/B cycles | **Healthy** | Theme, size, spacing, margins, tables, code, contrast, and reading mode survive separate save/restart cycles. [Reading restarted](images/ui-ux-audit-2026-07-14/STATE-02-reading-restarted.png) |
| 35 | STATE-03 — sidecar faults/timing | **Concern** | Corrupt states safely fall back, but failures are silent and calculator permission parity is unverified. [Checksum fallback](images/ui-ux-audit-2026-07-14/STATE-03-checksum-fallback.png); [permission blocker](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-state-03-calculator-permissions) |
| 36 | ERR-01 — direct startup failures | **Broken** | Missing/unreadable/oversized launches hide the reason and show the built-in sample as though it were the requested document. [Direct unreadable](images/ui-ux-audit-2026-07-14/ERR-01-direct-unreadable.png) |
| 37 | ERR-01 — in-document failures | **Broken** | Oversized input preserves the old document, but empty/binary-like/unreadable cases use inconsistent commit and error semantics. [Empty document](images/ui-ux-audit-2026-07-14/ERR-01-in-document-empty.png); [binary-like input](images/ui-ux-audit-2026-07-14/ERR-01-in-document-malformed.png) |
| 38 | ERR-02 — font failures | **Broken** | Invalid/truncated files restore the prior role; oversized files name the wrong error, while unsupported-outline/missing-race cases remain blocked. [Oversized CJK error](images/ui-ux-audit-2026-07-14/ERR-02-cjk-oversized.png); [fixture blocker](UI_UX_AUDIT_BLOCKERS_2026-07-14.md#blocked-err-02-font-fixtures) |
| 39 | ERR-03 — links/anchors | **Concern** | Valid and failed targets have visible outcomes, but only the first link in a multi-link block is keyboard-addressable. [Multiple links](images/ui-ux-audit-2026-07-14/ERR-03-multiple-links.png) |
