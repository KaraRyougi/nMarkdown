# nMarkdown Phase 0

This file is used by the desktop bring-up harness.

- The portable core renders into a 320 x 240 RGB565 surface.
- Platform adapters translate input into semantic events.
- Markdown parsing and anti-aliased text arrive in later phases.

Use line and page events to scroll the synthetic document preview.
