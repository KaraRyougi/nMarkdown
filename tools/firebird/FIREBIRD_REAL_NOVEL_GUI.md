# Firebird GUI touchpad validation

This diagnostic validates forward full-page swipes through Firebird's actual
QML `MouseArea`; it does not call `touchpad_set_state()` from the test program.
The document is always the exact `红楼梦.txt.tns` file.

## Firebird patches

Apply the production touchpad fix and optional test harness to a Firebird 1.6
source checkout:

```sh
patch -d /path/to/firebird -p1 \
  < tools/firebird/patches/firebird-gui-touchpad.patch
patch -d /path/to/firebird -p1 \
  < tools/firebird/patches/firebird-real-novel-gui-harness.patch
```

The production patch:

- submits the touch origin on mouse press, before any coalesced move;
- treats Synaptics report bytes `0x0c` through `0x0f` as zero padding instead
  of logging four warnings per aligned TI-OS read.

The harness is active only when `FIREBIRD_REAL_NOVEL_GUI_TEST=1`. It sends 30
top-to-bottom mouse drags through the QML touchpad, waits for each displayed
page to advance, and emits `FIREBIRD_GUI_TEST/1` evidence to stdout.

## nMarkdown diagnostic

Build:

```sh
make ndless-firebird-real-novel
```

Transfer these exact files to `/documents/ndless`:

- `build/ndless-firebird-real-novel/nmarkdown-firebird-real-novel.tns`
- `~/Downloads/红楼梦.txt.tns`
- `~/Downloads/fusion-pixel-12px-proportional-zh_hans.ttf.tns`

The diagnostic refuses a novel with the wrong byte length or sampled identity.
The host verifier additionally requires the exact SHA-256 for both the novel
and font.

Launch Firebird with:

```sh
FIREBIRD_REAL_NOVEL_GUI_TEST=1 /path/to/firebird-emu
```

Capture stdout as `build/firebird-real-novel-gui/serial.log`, then verify:

```sh
node tools/firebird/verify-real-novel-gui.mjs
```

Acceptance requires at least 30 sequential GUI swipes, strictly increasing
source offsets and pages, a rendered/presented/visible transaction for every
page, no `040c`-`040f` TPAD warnings, and both application and GUI PASS markers.

Do not launch this diagnostic with Firebird's remote-debugger `exec` command.
That command injects an Ndless SWI into whichever TI-OS task is active and can
trip the OS watchdog. Open the transferred program through TI-OS Documents, or
reuse a persisted normal document-launch state.
