# Navigation design

This document defines the current nMarkdown navigation model for the
TI-Nspire CX/CX II touchpad and keyboard.

## Goals

- Let the reader choose which touchpad axis provides continuous scrolling.
- Keep all keyboard directions stable and predictable.
- Preserve visual context across large movements so the reader does not have
  to reacquire the last line after every step.
- Avoid synthetic pagination for reflowed Markdown, where fonts, margins,
  formulas, and lazy layout can change page boundaries.

## Touchpad modes

The persisted **Touchpad mode** setting has two values:

| Mode | Continuous axis | Screen-step axis |
|---|---|---|
| Vertical Scroll | Vertical drag | Horizontal swipe |
| Horizontal Scroll | Horizontal drag | Vertical swipe |

The default is **Vertical Scroll**. Changing modes does not move or snap the
document; it changes only how future touchpad gestures are interpreted.

## Gesture directions

The persisted **Swipe gesture direction** setting applies only to the
screen-step axis:

| Setting | Advance gesture | Earlier gesture |
|---|---|---|
| Natural | Right or down | Left or up |
| Reversed | Left or up | Right or down |

Natural discrete swipes follow reading order: left-to-right or top-to-bottom
advances.

The independent persisted **Scroll gesture direction** setting applies only to
the continuous axis:

| Setting | Advance gesture | Earlier gesture |
|---|---|---|
| Natural | Up or left | Down or right |
| Reversed | Down or right | Up or left |

Natural continuous scrolling matches the content-follows-finger convention
used by macOS. Neither direction setting affects keys, modal lists,
clicks/taps, or direct horizontal manipulation of a focused wide block.

## Keyboard actions

- Up/8 and Down/2 always move one line earlier/later.
- Page Up/7 and Page Down/1/Tab always move one screen step earlier/later.
- Left/4 and Right/6 always move one screen step earlier/later, except when a
  focused wide block owns those keys for local panning.
- Search treats digits as query characters rather than navigation aliases.

## Boundary-aligned screen steps

A screen step uses the full 220-pixel viewport boundary:

1. Locate the visual line at the old viewport's bottom edge.
2. If that line is clipped and less than 85% was visible, place its top at the
   top of the new screen so the missing portion cannot be overlooked.
3. If the line was fully displayed or at least 85% visible, start at the first
   following line. A complete or effectively complete line is not repeated.
4. Record the exact old and new top offsets. An immediate reverse restores the
   exact old top rather than independently estimating a backward boundary.
5. Clamp the final step to the exact document end and guarantee monotonic
   movement through a formula or row taller than the viewport.

## Progress and header

Both modes traverse the same continuous, reflowed document. The two-pixel top
bar therefore uses:

```text
floor(320 * scroll_y / max_scroll_y)
```

The beginning is exactly 0%; only the exact end is 100%. A document that fits
within one viewport stays at 0%. No current/total page count is displayed, so
the filename can use the full header width and navigation does not depend on an
unstable page total.

## Acceptance checks

- Natural Vertical Scroll: drag up and swipe right advance.
- Reversed Vertical Scroll: drag down and swipe left advance.
- Natural Horizontal Scroll: drag left and swipe down advance.
- Reversed Horizontal Scroll: drag right and swipe up advance.
- Swipe and continuous-scroll directions can be changed independently.
- A physical gesture is applied once even though the input sampler emits both
  continuous deltas and one threshold swipe marker.
- Page Up returns to the preceding text screen with a complete row aligned at
  the physical top, even when the saved origin had a clipped top row.
- A TXT Page Down destination always top-aligns a complete first row, including
  when it repeats a row that intersected the preceding viewport.
- With TXT Auto spacing, a full viewport redistributes leading across a fitted
  row count so both the first ink top and final ink bottom are complete.
- Ordinary text repeats only a visual line that was clipped with less than 85%
  visible in the old viewport.
- Tall formulas and long documents remain reachable through the exact end.
- The progress formula and absence of a page label are identical in both modes.
- Persisted legacy mode byte 0 maps to Vertical Scroll; byte 1 maps to
  Horizontal Scroll.
