# Recommended architecture

Build the reader as a **native Ndless application**, with a portable rendering core shared by a desktop test harness.

That choice is driven by the requirements:

* The CX and CX II families use a 320 × 240, 16-bit-color display and have 64 MB of operating memory. The CX II has a 396 MHz processor, but the original CX should be treated as the performance baseline. ([德州仪器教育网站][1])
* Ndless supports native ARM C and C++ applications and exposes display, keyboard, touchpad, filesystem, and file-association APIs. Current upstream documentation lists support through OS 6.4.0.74, so calculator OS compatibility must be checked before installation. ([Ndless][2])
* A pure TI Lua implementation would be substantially constrained: the official handheld API exposes only built-in serif/sans-serif fonts at a small set of sizes, and the standard `file`, `io`, and `os` libraries are unavailable. That makes arbitrary Markdown-file access, bundled fonts, full Unicode coverage, and a custom anti-aliased renderer impractical. ([德州仪器教育网站][3])

A Lua preview could still exist for constrained environments, but it should not be the primary implementation.

---

## 1. Feature contract

“Unicode” and “LaTeX” should be defined carefully. Neither is a single binary feature.

| Area             | Version 1 commitment                                                                                                         | Later extension                                                              |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------- |
| Markdown         | CommonMark paragraphs, headings, emphasis, strong, links, lists, blockquotes, code, rules, tables, task lists, strikethrough | Footnotes, images, admonitions                                               |
| Math delimiters  | `$...$` and `$$...$$`                                                                                                        | `\(...\)` and `\[...\]`                                                      |
| LaTeX            | Mathematical LaTeX subset                                                                                                    | Broader compatibility through an optional MicroTeX backend                   |
| Unicode encoding | Strict UTF-8 Markdown; TXT auto-detection for UTF-8/BOM, GBK, Shift-JIS, EUC-JP, and ISO-2022-JP                               | UTF-16 TXT or a user-selectable override for intrinsically ambiguous streams |
| Scripts          | Latin, Greek, Cyrillic, mathematical symbols; CJK with a separately selected font                                             | Arabic, Hebrew, Indic and other complex scripts through a shaping/BiDi layer |
| Combining marks  | Precomposed characters plus practical combining-mark placement                                                               | Full normalization and OpenType positioning                                  |
| Emoji            | Monochrome glyphs when present in a font                                                                                     | No color emoji in the proposed baseline                                      |
| Anti-aliasing    | Grayscale A8 glyph masks blended into RGB565                                                                                 | Optional tuned stem darkening or supersampling                               |
| Large files      | Direct parsing below a measured memory cap                                                                                   | Split into linked raw Markdown chapters                                      |

A key rule is:

> UTF-8 decoding and glyph coverage do not constitute complete Unicode text layout.

Arabic, Indic scripts, bidirectional paragraphs, ligatures, contextual substitutions, and complex mark attachment require a shaping and BiDi subsystem. The architecture should support that subsystem later, but version 1 should not silently render those scripts incorrectly and call it “full Unicode.”

---

# 2. High-level data flow

```text
Raw Markdown or TXT file
             │
             ▼
 Source manager + bounded TXT decoder
             │
             ▼
 Markdown parser or literal TXT builder
             │
             ▼
   Compact document intermediate representation
       │                            │
       │                            └── Heading/link/search indexes
       ▼
 Lazy block and inline layout
       │
       ├──────────────┐
       ▼              ▼
 Text shaping      TeX math parsing/layout
       │              │
       └──────┬───────┘
              ▼
       Layout box tree
              │
              ▼
      Glyph and math caches
              │
              ▼
   A8 coverage-mask compositor
              │
              ▼
       RGB565 back buffer
              │
              ▼
          lcd_blit()
```

Recommended source tree:

```text
mdreader/
├── app/
│   ├── main.cpp
│   ├── viewer.cpp
│   ├── navigation.cpp
│   └── settings.cpp
├── platform/
│   ├── platform.h
│   ├── ndless/
│   │   ├── display_ndless.cpp
│   │   ├── input_ndless.cpp
│   │   ├── files_ndless.cpp
│   │   └── clock_ndless.cpp
│   └── desktop/
│       ├── display_desktop.cpp
│       └── input_desktop.cpp
├── document/
│   ├── source.cpp
│   ├── utf8.cpp
│   ├── markdown.cpp
│   ├── document_ir.cpp
│   └── index.cpp
├── layout/
│   ├── block_layout.cpp
│   ├── inline_layout.cpp
│   ├── line_break.cpp
│   ├── table_layout.cpp
│   └── style.cpp
├── text/
│   ├── font.cpp
│   ├── font_fallback.cpp
│   ├── harfbuzz_shaper.cpp
│   ├── glyph_cache.cpp
│   └── unicode_props.cpp
├── math/
│   ├── math_lexer.cpp
│   ├── math_parser.cpp
│   ├── math_atoms.cpp
│   ├── math_boxes.cpp
│   └── math_font.cpp
├── render/
│   ├── surface565.cpp
│   ├── blend.cpp
│   ├── draw_list.cpp
│   └── clip.cpp
├── third_party/
│   ├── md4c/
│   └── freetype/
└── tools/
    └── fontpack/
```

The platform-independent core should never call Ndless directly. It should depend on interfaces such as `Display`, `Input`, `File`, and `Clock`. That makes the renderer testable on a desktop and greatly reduces hardware-debugging time.

---

# 3. Ndless platform layer

## Display initialization

The current Ndless API defines 320 × 240 RGB565 and rotated RGB565 screen types, together with `lcd_init`, `lcd_blit`, and `lcd_type`. It also exposes key, touchpad, relative-path, and file-extension functions. ([GitHub][4])

Use a logical 320 × 240 coordinate system throughout the core:

```cpp
struct Surface565 {
    uint16_t* pixels;
    int width;       // 320
    int height;      // 240
    int stride;      // pixels per row
};
```

A full RGB565 buffer occupies:

```text
320 × 240 × 2 bytes = 153,600 bytes = 150 KiB
```

That is small enough to keep one permanent application back buffer. The
implemented modal fast path keeps one additional clean-frame snapshot of the
same size. It restores the unobscured reader beneath menus, loading cards, and
file/font browsers, and is invalidated by any base-content change. It is not a
page-turn cache.

Startup and shutdown should resemble:

```cpp
int main(int argc, char** argv) {
    enable_relative_paths(argv);

    DisplayNdless display;
    if (!display.initialize()) {
        return 1;
    }

    int result = run_reader(argc, argv, display);

    display.shutdown();
    return result;
}
```

And inside `DisplayNdless`:

```cpp
bool DisplayNdless::initialize() {
    native_type_ = lcd_type();

    // The core still renders into a logical 320x240 RGB565 surface.
    if (!lcd_init(SCR_320x240_565)) {
        return false;
    }

    framebuffer_.resize(320 * 240);
    return true;
}

void DisplayNdless::present() {
    lcd_blit(framebuffer_.data(), SCR_320x240_565);
}

void DisplayNdless::shutdown() {
    // Restore the OS display state.
    lcd_init(SCR_TYPE_INVALID);
}
```

The platform backend should explicitly test rotated/native display variants. The core should remain unaware of rotation; either Ndless performs the conversion or `DisplayNdless` writes through a rotated pixel-addressing function.

Do not access the old screen address directly. The current header explicitly directs applications toward `lcd_blit`. ([GitHub][4])

## Event loop

The application should be event-driven rather than continuously redrawing:

```cpp
while (!viewer.quit_requested()) {
    InputEvent event;

    bool received_event = input.poll(event);
    bool changed = received_event && viewer.handle_event(event);

    changed |= viewer.perform_incremental_work(/* time budget */);

    if (changed || viewer.is_dirty()) {
        viewer.layout_visible_region();
        viewer.render(display.surface());
        display.present();
        viewer.clear_dirty();
    } else {
        idle();
    }
}
```

This preserves battery life and avoids spending CPU time rendering an unchanged page.

Input handling should convert calculator-specific state into semantic events:

```cpp
enum class InputEventType {
    ScrollLineUp,
    ScrollLineDown,
    PageUp,
    PageDown,
    PanLeft,
    PanRight,
    Activate,
    Back,
    OpenMenu,
    OpenSearch,
    IncreaseFont,
    DecreaseFont
};
```

Suggested mapping:

* Up/down: line scrolling.
* Shift or Ctrl plus up/down: context-preserving screen steps.
* Left/right: earlier/later screen steps, or horizontal panning when a wide formula, table, or code block has focus.
* Enter: follow link or focus a wide block.
* Escape: leave focus, close overlay, then exit.
* Doc: table of contents and bookmarks for the current document.
* Menu: reader settings (with Ctrl+T as an equivalent shortcut).
* Touchpad navigation: Vertical Scroll assigns continuous motion to the vertical
  axis and screen steps to horizontal swipes; Horizontal Scroll swaps those
  roles. A persisted Natural/Reversed setting applies to both gesture axes.
  Natural makes up/left advance, while Reversed makes down/right advance. Keys,
  taps/clicks, modal gestures, and wide-content panning keep fixed mappings.
  Direction classification compares the physical-scale native X/Y deltas;
  dividing by each full range would distort the rectangular pad into a square.
  A short tap slop (about 1.25% of the shorter native span), a larger common
  drag-start deadzone (about 4%), and a separate motion deadzone reduce noise.
  A drag locks only when one axis leads the other by at least 3:2 and may not
  change axes before release; ignored sub-threshold samples accumulate.
* The discrete swipe axis uses the same context-preserving resolver as keyboard
  screen-step actions: retain one complete visual line, remember the exact
  reverse position, and fall back to about 85% of the viewport for oversized
  content that cannot supply line context.

Implement key repeat yourself with separate initial-delay and repeat intervals. Do not derive repeat behavior directly from frame rate.

## Opening documents

Register a Markdown-related extension through `cfg_register_fileext`, and accept the opened path through `argv`. Keep a built-in file browser as a fallback.

Do not make the rest of the application depend on how the TI document browser represents transferred filenames. Normalize that behavior in `files_ndless.cpp`.

---

# 4. Markdown parsing and document representation

## Parser recommendation: MD4C

MD4C is a good fit because it is implemented as one C source file plus one header, uses a callback/push model instead of constructing its own large DOM, expects UTF-8 by default, and supports CommonMark 0.31. Its extensions include tables, task lists, strikethrough, and explicit `$`/`$$` LaTeX math spans. ([GitHub][5])

Recommended parser flags:

```cpp
constexpr unsigned kMarkdownFlags =
    MD_FLAG_TABLES |
    MD_FLAG_TASKLISTS |
    MD_FLAG_STRIKETHROUGH |
    MD_FLAG_LATEXMATHSPANS |
    MD_FLAG_NOHTMLBLOCKS |
    MD_FLAG_NOHTMLSPANS;
```

Raw HTML should initially be disabled. Implementing a browser-like HTML renderer is outside the reader’s scope and creates difficult security and layout questions.

MD4C processes a complete supplied document buffer. It also deliberately does not validate malformed UTF-8, so validation must happen before parsing. ([GitHub][5])

## Direct-document mode

For ordinary files:

1. Read the file into one transient buffer.
2. Strip an optional UTF-8 BOM.
3. Validate and sanitize UTF-8.
4. Invoke `md_parse`.
5. Convert callbacks into your own compact intermediate representation.
6. Record source offsets rather than copying normal text wherever possible.
7. Release the full input buffer after parsing.
8. Retrieve text later through a small page cache backed by the source file.

Because a callback pointer is not necessarily guaranteed to belong to the original source in every situation, use a defensive check:

```cpp
TextStorage store_text(
    const char* parse_buffer,
    size_t parse_size,
    const char* text,
    size_t text_size)
{
    if (text >= parse_buffer &&
        text + text_size <= parse_buffer + parse_size) {
        return SourceSlice{
            .offset = static_cast<uint32_t>(text - parse_buffer),
            .length = static_cast<uint32_t>(text_size)
        };
    }

    return OwnedString{string_arena.copy(text, text_size)};
}
```

Set a measured direct-parsing limit rather than assuming the entire 64 MB is available. A reasonable initial policy is:

* Original CX: 4 MiB raw Markdown limit.
* CX II: 8 MiB raw Markdown limit.
* Larger documents: reject with a clear message and split them into linked raw Markdown chapters on the desktop.

These are engineering defaults, not hardware limits.

## Compact document IR

Avoid a general-purpose object graph with one heap allocation per node. Use packed arrays and integer indexes.

```cpp
using NodeId = uint32_t;
using TokenId = uint32_t;

enum class BlockKind : uint8_t {
    Paragraph,
    Heading,
    CodeBlock,
    Quote,
    UnorderedList,
    OrderedList,
    ListItem,
    HorizontalRule,
    Table,
    TableRow,
    TableCell,
    DisplayMath
};

struct BlockRecord {
    BlockKind kind;
    uint8_t depth;
    uint16_t flags;

    NodeId parent;
    NodeId first_child;
    NodeId next_sibling;

    TokenId first_token;
    uint32_t token_count;

    uint32_t source_offset;
    uint32_t source_length;

    int32_t estimated_height_26_6;
    int32_t measured_height_26_6;
};
```

Inline tokens can be similarly packed:

```cpp
enum class InlineKind : uint8_t {
    Text,
    SoftBreak,
    HardBreak,
    EmphasisStart,
    EmphasisEnd,
    StrongStart,
    StrongEnd,
    Code,
    LinkStart,
    LinkEnd,
    InlineMath,
    ImagePlaceholder
};

struct InlineToken {
    InlineKind kind;
    uint8_t style_flags;
    uint16_t aux;
    uint32_t source_offset;
    uint32_t source_length;
};
```

Use three allocation domains:

* **Persistent arena:** document nodes, tokens, headings, links.
* **Cache allocator:** glyphs, measured layouts, formula results.
* **Frame arena:** temporary line boxes and draw commands, reset after every render.

This prevents heap fragmentation.

## Entity handling

MD4C reports entities to the application rather than necessarily decoding them. Implement:

* Numeric decimal entities.
* Numeric hexadecimal entities.
* A build-generated compact table or trie for named CommonMark/HTML entities.
* U+FFFD for invalid scalar values.

Preserve original byte offsets so search results, bookmarks, and error messages can refer back to the source.

---

# 5. Virtual layout and scrolling

Parsing the document and laying out the document are separate operations.

Never fully lay out a large book during opening or a page-turn input. The
synchronous path measures only blocks intersecting the visible viewport.
Reject off-screen shaped lines before glyph rasterization and compositing, even
when their containing block intersects the viewport. Split literal TXT into
visually continuous eight-line layout units so an input frame cannot inherit a
128-line chunk cliff. Repeated idle polls advance at most one future shaped line
through the next three viewports, keeping layouts inside the same 96-block LRU
cache and warming no more than 48 text glyphs per poll in the existing A8 atlas.
Do not allocate page-sized RGB565 bitmaps: three 320x220 cached pages alone
would cost about 413 KiB and would become stale after theme, font, spacing,
search, or focus changes.

Page Up/Down first computes the nominal 220-pixel destination, then resolves it
to a measured line top. If the old bottom edge crossed a line and at most 85%
of that line was visible, Page Down uses it as the new first fully visible
line. If more than 85% was visible, it starts at the following line. The test
is strict integer arithmetic, so exactly 85% is retained. A formula line taller
than the viewport is the only unsnapped case, and must still make monotonic
progress. Current/total page display is constant-time arithmetic over the
height index and must never trigger whole-document layout.

Treat the configured side margin as the canonical outer content box:
`[margin, 320 - margin)`. Prose, headings, display-math centering, rules, and
overflow affordances use those exact symmetric bounds. Decorated code and table
blocks may add equal internal padding, but must not subtract it only from the
right edge or shift their background outside the configured box.

## Height index

Each top-level layout unit has either an estimated or measured height. Maintain a Fenwick tree over those heights:

```text
block 0 height
block 1 height
block 2 height
...
```

This supports:

* Prefix height lookup in `O(log n)`.
* Converting a document scroll position into a block index in `O(log n)`.
* Updating a block when its measured height replaces its estimate in `O(log n)`.

When a block above the current viewport changes height, preserve an anchor:

```cpp
struct ViewAnchor {
    NodeId block;
    int32_t local_y_26_6;
    uint32_t source_offset;
};
```

Recompute global scroll position from the anchor after an update. Otherwise, the page will visibly jump while lazy layout fills in exact heights.

## Fixed-point coordinates

Use 26.6 fixed-point values:

```cpp
using Fx = int32_t;

constexpr Fx fx_from_int(int x) { return x << 6; }
constexpr int fx_floor(Fx x)    { return x >> 6; }
```

Benefits:

* Deterministic output across CX and CX II.
* Fractional font advances without requiring floating-point operations in hot loops.
* Subpixel glyph origins can be represented when useful.
* Easy conversion to final integer pixel coordinates.

## Inline line layout

Convert each paragraph into inline items:

```text
glyph run
space/glue
inline math box
inline code box
hard break
soft break
link boundary
```

Each item reports:

```cpp
struct InlineMetrics {
    Fx width;
    Fx ascent;
    Fx descent;
    bool break_before;
    bool break_after;
};
```

Use a bounded greedy line composer suited to the narrow display:

1. Add items until the next item exceeds available width.
2. Break at the last allowed opportunity.
3. If no legal break exists, split a text run at a codepoint boundary.
4. Never split a UTF-8 sequence.
5. Prefer not to split a grapheme cluster.
6. Recompute line ascent and descent from all text and inline-math boxes.
7. Defer collapsible whitespace until the following item fits, so wrapped lines
   never retain invisible trailing-space runs.
8. Apply opening/closing punctuation constraints when a long unspaced run must
   be divided.

Greedy breaking is predictable and sufficiently fast for a 320-pixel display. A more sophisticated paragraph optimizer would have little visual room to demonstrate its advantages.

## Suggested default style metrics

These are starting values to tune on hardware:

```text
Viewport:              320 × 240
Horizontal page margin: 8 px
Usable text width:      304 px
Body em size:           15 px
Body line height:       18 px
Inline code size:       14 px
H1 size:                24 px
H2 size:                21 px
H3 size:                18 px
Paragraph bottom gap:    7 px
List indent:            14 px per level
Quote indicator:         2 px rule + 6 px gap
```

Keep a layout signature:

```cpp
struct LayoutSignature {
    uint16_t content_width;
    uint16_t body_px;
    uint16_t line_height_px;
    uint16_t font_pack_version;
    uint8_t table_mode;
    uint8_t code_wrap;
};
```

Cached measurements are valid only when their signature matches.

## Tables on a narrow display

A desktop-style table often cannot fit in 304 pixels. Support two modes:

**Responsive mode, default:** Render each body row as a vertical record:

```text
Name:       Fourier transform
Complexity: O(n log n)
Notes:      In-place variant available
```

**Grid mode:** Preserve rows and columns and let the selected table pan horizontally. Left/right input pans the table rather than the page while the table is focused.

## Code blocks

Use a monospaced font and preserve spaces. Recommended defaults:

* Wrap source code to the available width by default.
* Offer independent horizontal panning through a setting.
* Draw a subtle border or contrasting background.
* Show a right-edge continuation marker.
* Allow restoring unwrapped Pan mode through the setting.

## Wide display equations

Do not automatically break arbitrary math at normal operators in version 1. Instead:

1. Center the equation when it fits.
2. Honor explicit `\\` line breaks.
3. If it does not fit, provide horizontal panning.
4. Optionally scale it down only to a configured minimum, such as 80%.
5. Show a small overflow indicator when the block is not focused.

---

# 6. Unicode and font architecture

## Font rasterizer

Use a **minimal FreeType build** for the text backend. Compile only the SFNT,
TrueType, CFF/CFF2, hinting, and grayscale raster modules needed for unchanged
in-memory `.ttf`/`.otf` files. HarfBuzz remains responsible for shaping;
FreeType supplies low-resolution metrics and A8 outline rasterization. ([FreeType][6])

Wrap it behind an internal interface:

```cpp
class FontFace {
public:
    virtual GlyphId glyph_for_codepoint(uint32_t cp) const = 0;
    virtual GlyphMetrics metrics(GlyphId glyph, Fx px_size) = 0;
    virtual Kerning kerning(
        GlyphId left,
        GlyphId right,
        Fx px_size) = 0;
    virtual GlyphBitmap rasterize(
        GlyphId glyph,
        Fx px_size,
        uint8_t subpixel_phase) = 0;
};
```

Do not let layout code include FreeType headers.

## External fonts

Shipping a universal built-in font is undesirable, especially for CJK. Load
ordinary font files directly instead of defining a second user-facing package
format or requiring a converter. Recursively discover `.ttf`, `.otf`, and their
calculator-wrapped `.tns` names below My Documents.

The settings menu first selects a role and then a file:

```text
Body       embedded DejaVu Sans by default; used for prose
Monospace  embedded DejaVu Sans Mono by default; used for inline and block code
CJK        empty by default; load Sarasa Fixed SC as the fallback face
Math       embedded Latin Modern Math; fixed for all formula rendering
```

Each selected font remains independent and active while documents are switched
in the same session. Persist the chosen CJK path in a small hidden, checksummed
preference below My Documents and restore it on the next launch; invalid or
missing saved fonts fall back without blocking startup. Body and Monospace
remain session choices. A CJK face must not capture Latin prose merely because its
`cmap` also contains Latin glyphs. Resolve the preferred role first, then walk a
role-aware fallback chain ending in the embedded replacement glyph.

HarfBuzz consumes the untouched font's OpenType tables and returns glyph IDs,
advances, offsets, and clusters. The rasterizer accepts TrueType, CFF, and CFF2
OpenType outlines from memory. Preserve HarfBuzz's fractional
origins in the glyph cache so low-resolution rounding cannot introduce false
spaces inside words.

The replacement glyph must always be present.

## UTF-8 decoder

The UTF-8 decoder should:

* Reject overlong encodings.
* Reject surrogate values.
* Reject codepoints above U+10FFFF.
* Replace each malformed subsequence with U+FFFD.
* Always make forward progress.
* Preserve original byte offsets.

Example API:

```cpp
struct DecodedCodepoint {
    uint32_t value;
    uint32_t byte_offset;
    uint8_t byte_length;
    bool valid;
};

DecodedCodepoint utf8_next(
    std::span<const uint8_t> bytes,
    uint32_t offset);
```

## HarfBuzz shaping pipeline

Runtime shaping:

1. Strictly decode UTF-8 while preserving source-byte clusters.
2. Resolve fallback faces and segment contiguous face/script runs.
3. Shape each run with HarfBuzz's OpenType backend, including GSUB, GPOS,
   kerning, ligatures, mark attachment, and glyph reordering.
4. Keep required ligatures intact; when optional tracking is enabled, disable
   discretionary ligatures and add spacing only between shaped clusters.
5. Emit HarfBuzz glyph IDs and 26.6 positions for FreeType rasterization.

```cpp
struct PositionedGlyph {
    FontFaceId face;
    GlyphId glyph;
    Fx x_advance;
    Fx y_advance;
    Fx x_offset;
    Fx y_offset;
    uint32_t source_cluster;
};
```

Keep raw source bytes unchanged. Canonical-equivalence-aware search requires generated normalization tables in the runtime rather than an offline document conversion step.

## Future paragraph-direction interface

Design the simple shaper as an implementation of:

```cpp
class TextShaper {
public:
    virtual ShapeResult shape(
        std::span<const uint32_t> codepoints,
        TextDirection direction,
        Script script,
        const FontFallbackChain& fonts,
        Fx size) = 0;
};
```

A future paragraph layer can add:

* Paragraph-level bidirectional resolution.
* Unicode Bidirectional Algorithm resolution across mixed-direction runs.
* Language selection beyond the current neutral default.
* Vertical text, if a calculator use case emerges.

This isolates advanced Unicode work from Markdown and layout.

---

# 7. Anti-aliased rendering

## A8 glyph masks

Rasterize every glyph into an 8-bit coverage mask:

```cpp
struct GlyphBitmap {
    uint16_t width;
    uint16_t height;
    int16_t bearing_x;
    int16_t bearing_y;
    Fx advance;
    const uint8_t* coverage;
};
```

FreeType's normal grayscale renderer produces byte-per-pixel coverage suitable
for alpha masks. Apply a configurable 256-entry coverage transformation before
compositing so the low-resolution RGB565 result can be tuned independently.

```cpp
uint8_t corrected_alpha = coverage_lut[raw_coverage];
```

Tune separate LUTs for:

* Dark text on light background.
* Light text on dark background.
* Small text below a chosen pixel size.

## RGB565 blending

For a general foreground/background pair:

```cpp
static uint16_t blend565(
    uint16_t dst,
    uint16_t src,
    uint8_t alpha)
{
    int dr = (dst >> 11) & 31;
    int dg = (dst >> 5)  & 63;
    int db =  dst        & 31;

    int sr = (src >> 11) & 31;
    int sg = (src >> 5)  & 63;
    int sb =  src        & 31;

    int r = dr + ((sr - dr) * alpha + 127) / 255;
    int g = dg + ((sg - dg) * alpha + 127) / 255;
    int b = db + ((sb - db) * alpha + 127) / 255;

    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}
```

Most reader text is painted over a uniform background. Optimize that case by precomputing a shade palette for each active foreground/background pair:

```cpp
uint16_t text_palette[256];

for (int a = 0; a < 256; ++a) {
    text_palette[a] = blend565(background, foreground, coverage_lut[a]);
}
```

The hot glyph loop then becomes:

```cpp
for (int y = 0; y < glyph.height; ++y) {
    uint16_t* dst = surface.row(dst_y + y) + dst_x;
    const uint8_t* src = glyph.coverage + y * glyph.width;

    for (int x = 0; x < glyph.width; ++x) {
        uint8_t a = src[x];

        if (a == 0) {
            continue;
        } else if (a == 255) {
            dst[x] = foreground;
        } else {
            dst[x] = text_palette[a];
        }
    }
}
```

Use the general blend path over nonuniform backgrounds.

## Grayscale, not subpixel anti-aliasing

Use grayscale anti-aliasing exclusively. Subpixel rendering should be avoided because:

* Display subpixel order should not be assumed.
* Some framebuffer modes are rotated.
* Subpixel artifacts are conspicuous on colored backgrounds.
* Grayscale masks work identically for text and mathematical glyphs.

Use separate light/dark A8 coverage tables. On dark paper, apply a monotonic
fixed-point lift to mid-coverage values before RGB565 blending so white edges
remain legible after quantization; preserve coverage 0/255 exactly and keep the
low-coverage fringe subdued. Apply the same table to partially covered math
rules. Do not alter Light-mode output or allocate another framebuffer.

## Glyph cache

Use an A8 atlas cache:

```text
Atlas page:       256 × 256 × 1 byte = 64 KiB
Initial pages:    4
Initial capacity: 256 KiB
Eviction:         least recently used
```

Cache key:

```cpp
struct GlyphKey {
    FontFaceId face;
    GlyphId glyph;
    uint16_t pixel_size_26_6;
    uint8_t x_phase;
    uint8_t render_flags;
};
```

Store:

* Atlas coordinates.
* Bearings.
* Advance.
* Last-used generation.
* Font-pack version.

Use four quarter-pixel x phases for grayscale rasterization. Firebird visual
tests showed that whole-pixel snapping of HarfBuzz's fractional advances
produced false gaps inside words (for example, `fallb ack`). Keep the pen in
26.6, quantize each glyph origin to the nearest quarter pixel, and key the A8
cache by that phase. This is grayscale outline positioning, not RGB subpixel
rendering.

---

# 8. LaTeX math engine

## Recommended approach

Implement a dedicated **TeX mathematical-layout subset** for the first production version.

Do not implement a complete TeX engine. A Markdown reader needs mathematical formulas, not:

* Package loading.
* General file input.
* Counters.
* Page construction.
* TikZ.
* Arbitrary macro programming.
* TeX output routines.

A custom mathematical engine offers predictable memory use, startup cost, error recovery, and display behavior on the original CX.

Keep the backend abstract so a more compatible engine can be added later:

```cpp
class MathRenderer {
public:
    virtual MathLayoutResult layout(
        std::string_view latex_utf8,
        MathStyle style,
        Fx max_width,
        const MathTheme& theme) = 0;
};
```

## Optional MicroTeX backend

MicroTeX is a plausible second backend because it is intended as an embeddable LaTeX formula renderer. However, its upstream build requires C++17, loads external resources, and expects platform implementations of font, text-layout, and graphics interfaces. Its code is MIT-licensed, while fonts and XML resources have separate licenses. That makes it a porting and footprint experiment rather than the safest first implementation. ([GitHub][8])

A MicroTeX feasibility gate should require:

1. A headless build without GTK, Qt, Cairo, or desktop GUI code.
2. `Font`, `TextLayout`, and `Graphics2D` adapters over the existing font and RGB565 subsystems.
3. No unbounded resource loading.
4. Measured binary size and peak memory.
5. Measured formula startup and render latency on an original CX.
6. A full audit of bundled-resource licenses.

Until it passes those tests, keep it behind `MATH_BACKEND_MICROTEX`.

## Lexer

Token types:

```cpp
enum class MathTokenKind {
    End,
    Character,
    ControlSequence,
    BeginGroup,       // {
    EndGroup,         // }
    Superscript,      // ^
    Subscript,        // _
    AlignmentTab,     // &
    RowBreak,         // \\
    OptionalBegin,    // [
    OptionalEnd,      // ]
    Whitespace
};
```

Rules:

* Control words consume ASCII letters after `\`.
* Control symbols consume exactly one following codepoint.
* Whitespace following a control word is ignored.
* Ordinary Unicode characters remain Unicode tokens.
* Comments and full TeX category-code behavior are not supported.
* Enforce maximum source length and token count.

## Grammar

A simplified grammar:

```text
formula        := row EOF
row            := atom*
atom           := primary scripts?
scripts        := ('_' script_arg)? ('^' script_arg)?
                 | ('^' script_arg)? ('_' script_arg)?
script_arg     := group | primary
group          := '{' row '}'
primary        := character
                | symbol_command
                | fraction
                | radical
                | accent
                | style_command
                | delimiter_group
                | environment
```

## Math atom classes

Classify atoms as TeX-like categories:

```cpp
enum class AtomClass {
    Ordinary,
    Operator,
    Binary,
    Relation,
    Opening,
    Closing,
    Punctuation,
    Inner
};
```

Spacing is determined by a compact class-pair table and the current math style. This is essential for expressions such as:

```latex
a+b=c
f(x)
\sin x
x \in A
```

A renderer that merely places glyphs side by side will look incorrect even when every symbol exists.

## Box model

Every parsed expression becomes boxes:

```cpp
struct MathBoxMetrics {
    Fx width;
    Fx ascent;
    Fx descent;
};

class MathBox {
public:
    MathBoxMetrics metrics;
    virtual void emit_draw_commands(
        DrawList& out,
        Fx x,
        Fx baseline) const = 0;
};
```

Concrete boxes:

```text
GlyphBox
TextBox
HorizontalBox
VerticalBox
KernBox
RuleBox
FractionBox
RadicalBox
ScriptBox
DelimiterBox
AccentBox
LargeOperatorBox
ArrayBox
ErrorBox
```

The paragraph layout engine sees an inline formula simply as a box with width, ascent, and descent.

## Mathematical styles

Support four styles:

```text
Display
Text
Script
ScriptScript
```

Style transitions:

* Numerator and denominator: smaller style.
* Superscript and subscript: smaller style.
* Nested scripts: ScriptScript.
* Large operators: large in Display, compact in Text.
* Inline formulas begin in Text style.
* `$$...$$` begins in Display style.

Use font-provided constants where available, otherwise carefully tuned em-relative defaults.

## Font metadata

The runtime outline rasterizer should not be responsible for all mathematical layout knowledge.

The build-time math-font metadata tool should extract and serialize:

* Axis height.
* Fraction rule thickness.
* Numerator and denominator shifts and gaps.
* Superscript and subscript shifts.
* Script scale percentages.
* Radical rule thickness.
* Radical vertical gap.
* Delimiter shortfall.
* Large-operator spacing.
* Glyph italic corrections.
* Top-accent attachment points.
* Delimiter size variants.
* Delimiter assembly recipes.

Runtime file:

```cpp
struct MathFontConstants {
    int16_t axis_height;
    int16_t fraction_rule;
    int16_t fraction_num_gap;
    int16_t fraction_den_gap;
    int16_t superscript_shift;
    int16_t subscript_shift;
    int16_t radical_rule;
    int16_t radical_gap;
    // Values stored in font units.
};
```

## Required version 1 commands

Symbols and Greek:

```latex
\alpha \beta \gamma \delta \epsilon
\theta \lambda \mu \pi \rho \sigma
\phi \psi \omega
\Gamma \Delta \Theta \Lambda \Pi \Sigma \Phi \Psi \Omega

\pm \mp \times \div \cdot
\le \ge \ne \approx \equiv
\in \notin \subset \subseteq \supset \supseteq
\to \rightarrow \leftarrow \leftrightarrow
\Rightarrow \Leftarrow \Leftrightarrow
\infty \partial \nabla
```

Structures:

```latex
\frac{a}{b}
\sqrt{x}
\sqrt[n]{x}
x^2
x_i
x_i^2
\sum_{i=0}^{n}
\prod_{i=1}^{n}
\int_a^b
\lim_{x\to 0}
```

Delimiters:

```latex
\left( ... \right)
\left[ ... \right]
\left\{ ... \right\}
\left| ... \right|
\langle ... \rangle
```

Accents and decorations:

```latex
\hat{x}
\bar{x}
\vec{x}
\dot{x}
\ddot{x}
\overline{AB}
\underline{x}
```

Styles and text:

```latex
\mathrm{d}
\mathbf{x}
\mathit{x}
\mathbb{R}
\mathcal{F}
\text{for all }
```

Arrays:

```latex
\begin{matrix}
a & b \\
c & d
\end{matrix}

\begin{pmatrix} ... \end{pmatrix}
\begin{bmatrix} ... \end{bmatrix}
\begin{cases} ... \end{cases}
\begin{aligned} ... \end{aligned}
```

Spacing:

```latex
\, \: \; \! \quad \qquad
```

## Layout algorithms

### Fractions

1. Layout numerator and denominator in the next smaller style.
2. Center both horizontally.
3. Position the rule relative to the math axis.
4. Enforce minimum numerator/rule and rule/denominator gaps.
5. Ensure the rule is at least one physical pixel thick.

### Scripts

1. Place scripts after the base glyph’s advance plus italic correction.
2. Apply superscript and subscript shifts.
3. Enforce a minimum gap when both are present.
4. Move both apart if their boxes collide.
5. Use special limits placement above and below large operators in display style.

### Radicals

1. Layout the radicand.
2. Select the smallest radical glyph or assembly tall enough.
3. Draw the overbar across the radicand.
4. Position an optional root index at the upper-left shoulder.
5. Ensure the bar joins the radical glyph without a visible gap.

### Stretchy delimiters

1. Compute enclosed box height and depth.
2. Select a predesigned delimiter variant.
3. If no variant is large enough, assemble top, extender, middle, and bottom pieces.
4. Use vertical scaling only as a last-resort fallback.

### Arrays and matrices

1. Layout every cell.
2. Compute maximum width per column.
3. Compute maximum ascent/descent per row.
4. Apply column and row spacing.
5. Align columns according to the environment.
6. Add outer delimiters for `pmatrix`, `bmatrix`, and `cases`.

## Error recovery

Never abort the document because one equation is malformed.

Apply limits:

```text
Maximum formula bytes:       16 KiB
Maximum token count:          8,192
Maximum brace nesting:        64
Maximum macro expansion:      disabled initially
Maximum matrix dimensions:    32 × 32
Maximum generated box count:  16,384
```

On error, render an `ErrorBox` containing:

* A warning symbol.
* A shortened literal version of the formula.
* A source-offset diagnostic accessible from the menu.

Unknown commands should ordinarily be rendered as visible literal text such as `\unknown`, not discarded.

## Formula caching

Cache by:

```text
hash(
    formula source bytes,
    math style,
    pixel size,
    available width,
    combined font signature,
    renderer version
)
```

Cache levels:

* Parsed math atom tree.
* Laid-out box tree.
* Optional rasterized display-equation bitmap.

Prefer caching box trees. A box tree remains theme-independent and can be recolored without rerasterizing layout.

---

# 9. Rendering pipeline

The layout engine should emit a compact draw list:

```cpp
enum class DrawCommandType {
    FillRect,
    StrokeLine,
    Glyph,
    PushClip,
    PopClip
};

struct DrawGlyphCommand {
    GlyphCacheHandle glyph;
    int16_t x;
    int16_t y;
    uint16_t foreground565;
    uint16_t background565;
};
```

Rendering steps:

1. Clear the invalidated area.
2. Paint block backgrounds and quote/list decorations.
3. Paint selection and search highlights.
4. Paint glyph masks.
5. Paint fraction bars and other solid rules.
6. Paint links and focus indicators.
7. Paint overlays such as the top reading-progress track.
8. Blit the completed buffer.

The reading-progress track is 320 pixels wide and two pixels high. Calculate it
after the visible document pass because the measured document height grows lazily:

* Both touchpad modes: `floor(320 * scroll_y / max_scroll_y)`.
* If the denominator is zero, draw no reading fill.

This makes the beginning exactly 0% and reserves a full 320-pixel fill for the
exact end. A load failure may replace reading progress with a full-width red
status strip; it is not a reading-position value. Do not display a synthetic
page count and do not add a vertical scrollbar.

Maintain dirty rectangles inside the renderer, even if presentation ultimately blits the full framebuffer. Dirty rectangles reduce layout traversal and compositing work.

Use clipping at every block boundary, especially for independently panned formulas, code blocks, and tables.

---

# 10. Memory budget

A practical initial budget:

| Component                            |                 Target |
| ------------------------------------ | ---------------------: |
| RGB565 render buffer                 |                150 KiB |
| Retained clean frame for modal UI    |                150 KiB |
| Frame arena and draw list            |                128 KiB |
| Four A8 glyph-atlas pages            |                256 KiB |
| Glyph metadata                       |              32–64 KiB |
| Source page cache                    |             64–128 KiB |
| Document IR                          |              0.5–2 MiB |
| Visible layout cache                 |            0.5–1.5 MiB |
| Formula parse/layout cache           |             0.25–1 MiB |
| Core Latin/Greek/Cyrillic/math fonts |        Target ≤1.5 MiB |
| Optional standalone fallback font    |          Target ≤4 MiB |
| Raw source parsing buffer            | 4 MiB CX / 8 MiB CX II |
| Total steady-state target            |                ≤12 MiB |
| Total transient target               |                ≤20 MiB |

The operating-memory specification is 64 MB, but the application should not assume all of it is freely available. ([德州仪器教育网站][1])

Use LRU eviction in this order:

1. Rasterized formula bitmaps.
2. Offscreen layout blocks.
3. Old glyphs, excluding pinned UI glyphs.
4. Source-cache pages.

Never evict:

* Current viewport layout.
* Current and immediately adjacent glyph runs.
* Core replacement glyph.
* Navigation overlay glyphs.

---

# 11. Direct font loading

Markdown and literal plain TXT are the document formats. Optional fonts stay as
their original `.ttf` or `.otf` files; the reader does not wrap, subset, or
convert them.

Loading rules:

* Keep the core body/replacement face embedded and always available.
* Assign stable high face ids to the Body, Monospace, and CJK user slots.
* Recursively discover fonts and calculator-wrapped font names below My Documents.
* Present role selection before file selection, plus a role-appropriate reset entry.
* Cap raw fonts by model and bounds-check the SFNT directory before rasterizer use.
* Accept TrueType, CFF, and CFF2 outlines through the minimal FreeType backend.
* Keep selected fonts active while switching documents in the same session;
  additionally remember the CJK path across launches in the global font
  preference.
* On success, clear font-dependent caches, reflow, and restore the source-relative anchor.
* On a font or reflow failure, restore the previous role face and show a local error.

---

# 12. Persistence and navigation

## Reading position

Do not persist an absolute pixel scroll offset. It becomes invalid after changing font size or margins.

Persist:

```cpp
struct SavedPosition {
    uint64_t document_identity;
    uint32_t source_offset;
    NodeId nearest_block;
    uint16_t relative_position_0_65535;
};
```

When reopening:

1. Find the block containing the saved source offset.
2. Lay out that block.
3. Restore its relative vertical position.
4. Fall back to the nearest preceding block when the document changed.

## Sidecar state

Store:

* Reading position.
* Bookmarks.
* Theme.
* Font size.
* Code-wrap preference.
* Last selected heading.
* Search history, optionally.
* Document identity and source hash.

Write state atomically:

1. Write temporary file.
2. Flush and close.
3. Rename over the previous state.
4. Keep one backup generation if rename behavior is uncertain.

## Table of contents

Generate the table of contents from heading callbacks during Markdown parsing. Store:

```cpp
struct HeadingEntry {
    uint32_t source_offset;
    NodeId block;
    uint8_t level;
    TextStorage title;
};
```

Display a flattened hierarchical list and allow collapsed sections.

## Search

Search source chunks rather than rendered glyphs.

Version 1:

* Exact UTF-8 search.
* ASCII case-insensitive search.
* Result source offsets.
* Search snippets decoded safely around codepoint boundaries.

Later:

* Generated Unicode case-fold tables.
* Canonical-equivalence-aware search.
* Whole-word matching by Unicode category.

Search highlights should be produced by mapping source ranges back to glyph clusters in the visible layout.

## Links

Support:

* Internal heading anchors.
* Relative links to another Markdown document.
* Links to an asset or another Markdown document.

External web links cannot be opened meaningfully on the calculator; show the URL in a copy/view dialog instead.

---

# 13. Desktop tooling and test harness

The desktop build should use exactly the same:

* Markdown parser adapter.
* Document IR.
* Unicode decoder.
* Text layout.
* Math parser.
* Glyph cache.
* RGB565 renderer.

Only display, input, and filesystem adapters differ.

Render desktop test output at the physical calculator resolution and RGB565 precision. A normal high-resolution desktop preview can conceal problems caused by:

* 5-bit red/blue channels.
* 6-bit green.
* Small text.
* Tight vertical space.
* One-pixel rules.
* Quantized anti-aliasing.

## Automated tests

### Markdown

* CommonMark conformance corpus.
* Tables, lists, task items, nested blockquotes.
* Math delimiters inside and outside code.
* Literal and escaped dollar signs.
* Deep nesting and malformed syntax.

### UTF-8

* Every valid sequence length.
* Overlong encodings.
* Truncated sequences.
* Surrogates.
* Values above U+10FFFF.
* Embedded NUL.
* Combining marks.
* Mixed fallback fonts.

### Math

* Golden metric tests for each box type.
* Nested fractions and radicals.
* Scripts on large operators.
* Delimiter assemblies.
* Matrices and cases.
* Malformed commands and braces.
* Resource-limit enforcement.

### Rendering

* Golden RGB565 screenshots.
* Light and dark themes.
* Cache eviction.
* Clipping.
* Wide formula and code-block panning.
* Text over highlight backgrounds.

### Fuzzing

Fuzz independently:

* UTF-8 validator.
* Markdown-to-IR callback adapter.
* Math lexer/parser.
* Font-pack reader.

All binary formats must perform bounds checks before dereferencing offsets.

## Hardware testing matrix

At minimum:

* Original CX.
* Original CX CAS.
* CX II.
* CX II CAS.
* Native and rotated framebuffer variants encountered in supported hardware.
* Warm and cold glyph cache.
* Documents with and without CJK packs.
* Maximum direct-source size.
* Repeated open/close cycles to catch leaks.

---

# 14. Performance strategy

Optimize for the original CX first. The CX II can then use larger caches and more aggressive prefetching without changing behavior.

The most important optimizations are architectural:

1. Never lay out the whole document eagerly.
2. Never rasterize the same glyph repeatedly.
3. Never parse the same formula repeatedly.
4. Avoid one heap allocation per Markdown or math node.
5. Paint only dirty regions.
6. Do not continuously redraw an idle page.
7. Subset fonts.
8. Keep text and formula rendering on the same glyph pipeline.
9. Cache source in pages rather than retaining a second full copy.
10. Render visible content before performing look-ahead work.

Provisional acceptance targets on the original CX:

```text
Warm one-line scroll update:      under 50 ms
Warm page update:                 under 100 ms
Simple formula first layout:      under 30 ms
Complex matrix first layout:      under 150 ms
Cached formula layout:            under 5 ms
Idle CPU behavior:                event-driven
Steady-state memory:              under 12 MiB
```

These should be treated as profiling targets, not promises. Record actual measurements in a benchmark screen:

```text
document parse time
visible layout time
formula parse/layout time
glyph rasterizations
glyph-cache hit rate
layout-cache hit rate
present time
peak memory
```

---

# 15. Implementation phases

## Phase 0 — Native platform bring-up

Deliver:

* Ndless executable.
* RGB565 initialization and cleanup.
* One back buffer.
* Key and touchpad events.
* File opening from `argv`.
* Desktop platform adapter.

Acceptance:

* Draw solid colors and primitives.
* Scroll a synthetic page.
* Exit without corrupting the TI OS display.

## Phase 1 — Unicode text and anti-aliasing

Deliver:

* Strict UTF-8 decoder.
* Font pack loader.
* Minimal FreeType wrapper.
* Fallback resolution.
* A8 glyph cache.
* RGB565 anti-aliased compositor.
* Basic LTR shaping and kerning.

Acceptance:

* Latin, Greek, Cyrillic, symbols, and replacement glyphs render correctly.
* Light and dark themes produce readable text.
* Cache activity stabilizes while scrolling repeated text.
* Invalid UTF-8 never crashes or stalls.

## Phase 2 — Markdown and virtual layout

Deliver:

* MD4C adapter.
* Compact document IR.
* Lazy block layout.
* Fenwick height index.
* Paragraphs, headings, lists, quotes, code, links.
* TOC and reading-position persistence.

Acceptance:

* CommonMark test documents render consistently.
* Opening a long document does not require full layout.
* Font-size changes preserve approximate reading position.
* Scrolling remains responsive after cache eviction.

## Phase 3 — Mathematical LaTeX

Deliver:

* Math lexer and parser.
* Atom classification.
* Box model.
* Scripts, fractions, radicals, operators, delimiters, accents, matrices.
* Math font metadata packer.
* Inline baseline integration.
* Formula cache and error boxes.

Acceptance:

* A defined formula corpus matches desktop golden output.
* Malformed formulas produce local errors, not application failures.
* Oversized equations can be panned.
* Inline formulas participate correctly in line ascent and descent.

## Phase 4 — Reader usability

Deliver:

* Search.
* Bookmarks.
* Table modes.
* Wide-block focus.
* Settings.
* Direct Markdown document browser and file associations.
* Role-based browser and direct loading for untouched TTF/glyf-OTF files.
* Crash-safe sidecar writes.

Acceptance:

* Raw Markdown within the model-specific source cap opens directly; oversized files fail clearly.
* Search results navigate to exact source positions.
* Bookmarks survive reflow and document reopen.
* A standalone CJK fallback remains within the defined memory budget and survives document switches.
* Changing or rejecting a font preserves the current document and source-relative position.

## Phase 5 — Advanced compatibility

Candidate work:

* Full paragraph-level bidirectional text.
* Footnotes and images.
* Optional MicroTeX backend.
* More advanced formula line breaking.
* User-defined safe macro aliases.
* Accessibility-oriented high-contrast themes.

---

# 16. Concrete technology selection

The recommended production stack is:

```text
Application platform:   Ndless native C/C++
Markdown parser:        MD4C
UTF-8 validation:       Project-owned strict decoder
Font rasterization:     Minimal FreeType (TrueType + CFF/CFF2 + grayscale)
Text shaping:           HarfBuzz OpenType shaper + project fallback segmentation
Math rendering v1:      Project-owned TeX math parser and box engine
Math rendering later:   Optional MicroTeX backend
Framebuffer:            320 × 240 RGB565
Anti-aliasing:          Cached A8 glyph masks with coverage LUT
Document scaling:       Lazy block layout + Fenwick height index
Large-book workflow:    Linked raw Markdown chapters
Optional font workflow: Direct TTF/OTF selection by role
Testing:                Shared desktop RGB565 renderer and hardware tests
```

Licensing should be tracked from the beginning: MD4C uses MIT, FreeType uses
the FreeType Project License, HarfBuzz uses its permissive upstream license,
and MicroTeX’s code is MIT while its bundled font/XML resources carry separate
licenses. Any embedded text or mathematics font also needs explicit
redistribution rights and attribution. ([GitHub][5])

This design gives the original CX a realistic performance baseline, provides genuinely anti-aliased custom-font rendering, supports a well-defined and extensible Unicode model, and keeps LaTeX mathematics integrated with normal paragraph layout rather than treating equations as pre-rendered screenshots.

[1]: https://education.ti.com/en/products/calculators/graphing-calculators/ti-nspire-cx-ii-cx-ii-cas/specifications "TI-Nspire™ Specifications"
[2]: https://ndless.me/ "Ndless for TI-Nspire"
[3]: https://education.ti.com/download/en/ed-tech/59108CCE54484B76AF68879C217D47B2/C637693253EA4B8A9655453E7F07721A/TI-Nspire%20Lua%20Scripting%20API%20Reference%20Guide.pdf "TI-Nspire(TM) Lua Scripting API Reference Guide"
[4]: https://github.com/ndless-nspire/Ndless/blob/master/ndless-sdk/include/libndls.h "Ndless/ndless-sdk/include/libndls.h at master · ndless-nspire/Ndless · GitHub"
[5]: https://github.com/mity/md4c "GitHub - mity/md4c: C Markdown parser. Fast. SAX-like interface. Compliant to CommonMark specification. · GitHub"
[6]: https://freetype.org/freetype2/docs/design/design-4.html "FreeType modular design"
[7]: https://freetype.org/freetype2/docs/reference/ft2-module_management.html "FreeType module management"
[8]: https://github.com/NanoMichael/MicroTex "GitHub - NanoMichael/MicroTeX: A dynamic, cross-platform, and embeddable LaTeX rendering library · GitHub"
