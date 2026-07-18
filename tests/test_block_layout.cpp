#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "nmarkdown/document/markdown.h"
#include "nmarkdown/layout/block_layout.h"
#include "nmarkdown/math/math_system.h"
#include "nmarkdown/platform/platform.h"
#include "nmarkdown/text/text_system.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n",             \
                         __FILE__, __LINE__, #condition);                      \
            ++failures;                                                        \
        }                                                                      \
    } while (false)

bool read_font_asset(const char* name, std::vector<std::uint8_t>& bytes) {
    const std::string path = std::string(NMARKDOWN_SOURCE_DIR) +
                             "/assets/fonts/" + name;
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) return false;
    bytes.assign(std::istreambuf_iterator<char>(input),
                 std::istreambuf_iterator<char>());
    return !bytes.empty();
}

void test_lazy_layout_and_anchor() {
    std::string source = "# Long document\n\n";
    for (int index = 0; index < 1000; ++index) {
        source += "Paragraph " + std::to_string(index) +
                  " has enough words to exercise deterministic wrapping on the narrow screen.\n\n";
    }

    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(document, text, signature, error));
    CHECK(layout.unit_count() == 1001);
    CHECK(layout.measured_count() == 0);

    const nmarkdown::HeadingEntry& heading = document.ir.headings.front();
    CHECK(layout.position_for_source(heading.block, heading.source_offset, 0) == 0);
    CHECK(layout.measured_count() == 0);  // Top-only jumps never block on shaping.

    const nmarkdown::Fx initial_total = layout.total_height();
    const nmarkdown::ViewAnchor anchor = layout.anchor_at(nmarkdown::fx_from_int(12000));
    const auto visible = layout.layout_window(nmarkdown::fx_from_int(12000),
                                               nmarkdown::fx_from_int(212));
    CHECK(!visible.empty());
    CHECK(layout.measured_count() > 0);
    CHECK(layout.measured_count() < 80);
    CHECK(layout.cache_size() <= 96);
    CHECK(layout.position_of(anchor) >= 0);
    CHECK(layout.total_height() != initial_total);

    for (std::size_t index = 0; index < layout.unit_count(); ++index) {
        CHECK(layout.layout_unit(index) != nullptr);
    }
    CHECK(layout.measured_count() == layout.unit_count());
    CHECK(layout.cache_size() <= 96);
    const nmarkdown::Fx measured_total = layout.total_height();
    CHECK(layout.layout_unit(0) != nullptr);  // Re-layout after LRU eviction.
    CHECK(layout.total_height() == measured_total);

    const nmarkdown::ViewAnchor reflow_anchor =
        layout.anchor_at(nmarkdown::fx_from_int(9000));
    signature.body_px = 19;
    signature.line_height_px = 23;
    CHECK(layout.reconfigure(signature, error));
    CHECK(layout.measured_count() == 0);
    CHECK(layout.position_of(reflow_anchor) >= 0);
}

void test_inline_and_display_math_integration() {
    const std::string source =
        "Inline $E=mc^2$ and $\\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}$.\n\n"
        "$$\\begin{bmatrix}a&b\\\\c&d\\end{bmatrix}$$\n";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(document, text, signature, error, &math));
    bool inline_math = false;
    bool display_math = false;
    for (std::size_t index = 0; index < layout.unit_count(); ++index) {
        const nmarkdown::BlockLayout* block = layout.layout_unit(index);
        CHECK(block != nullptr);
        if (block == nullptr) continue;
        for (const nmarkdown::LayoutLine& line : block->lines) {
            for (const nmarkdown::LayoutRun& run : line.runs) {
                if (run.math == nullptr) continue;
                CHECK(run.math->valid);
                CHECK(run.math->diagnostic.empty());
                CHECK(line.baseline_y - run.math->metrics.ascent >= 0);
                CHECK(line.baseline_y + run.math->metrics.descent <= block->height);
                for (const nmarkdown::MathDrawRun& math_run : run.math->runs) {
                    for (const nmarkdown::PositionedGlyph& glyph :
                         math_run.glyphs.glyphs) {
                        CHECK(glyph.face == 3);
                    }
                }
                if (run.display_math &&
                    run.math->metrics.width <
                        nmarkdown::fx_from_int(signature.content_width)) {
                    const nmarkdown::Fx centered_width =
                        run.x * 2 + run.math->metrics.width;
                    const nmarkdown::Fx content_width =
                        nmarkdown::fx_from_int(signature.content_width);
                    const nmarkdown::Fx error = centered_width > content_width
                                                    ? centered_width - content_width
                                                    : content_width - centered_width;
                    CHECK(error <= 1);  // One 26.6 unit from halving is allowed.
                }
                inline_math = inline_math || !run.display_math;
                display_math = display_math || run.display_math;
            }
        }
    }
    CHECK(inline_math && display_math);
}

void test_automatic_content_aware_line_spacing() {
    const std::string source =
        "A normal text line.\n\n"
        "A tall inline formula $\\frac{x_1^2+y_1^2}{\\sqrt{a+b}}$ on this line.\n\n"
        "Another normal text line.\n";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    signature.line_height_px = 0;
    CHECK(layout.initialize(document, text, signature, error, &math));
    CHECK(layout.unit_count() == 3);

    nmarkdown::Fx normal_advance = 0;
    nmarkdown::Fx formula_advance = 0;
    for (std::size_t index = 0; index < layout.unit_count(); ++index) {
        const nmarkdown::BlockLayout* block = layout.layout_unit(index);
        CHECK(block != nullptr && block->lines.size() == 1);
        if (block == nullptr || block->lines.empty()) continue;
        const nmarkdown::LayoutLine& line = block->lines.front();
        CHECK(line.advance >= line.ascent + line.descent + nmarkdown::fx_from_int(3));
        if (index == 0) normal_advance = line.advance;
        if (index == 1) formula_advance = line.advance;
    }
    CHECK(formula_advance > normal_advance);

    signature.line_height_px = 26;
    CHECK(layout.reconfigure(signature, error));
    const nmarkdown::BlockLayout* manual = layout.layout_unit(0);
    CHECK(manual != nullptr && !manual->lines.empty());
    if (manual != nullptr && !manual->lines.empty()) {
        CHECK(manual->lines.front().advance >= nmarkdown::fx_from_int(26));
    }
}

void test_inline_math_never_overlaps_following_prose() {
    const std::string source =
        "Inline baseline: $a+b=c$, $x_i^2+y_j^2=r^2$, and "
        "$\\frac{1}{2}+\\sqrt{x}$ stay inside a normal line.\n";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    signature.content_width = 310;
    signature.line_height_px = 0;

    for (int pixel_size = 12; pixel_size <= 22; ++pixel_size) {
        signature.body_px = static_cast<std::uint16_t>(pixel_size);
        if (pixel_size == 12) {
            CHECK(layout.initialize(document, text, signature, error, &math));
        } else {
            CHECK(layout.reconfigure(signature, error));
        }
        const nmarkdown::BlockLayout* block = layout.layout_unit(0);
        CHECK(block != nullptr);
        if (block == nullptr) continue;
        bool saw_inline_math = false;
        for (const nmarkdown::LayoutLine& line : block->lines) {
            nmarkdown::Fx previous_right = 0;
            for (const nmarkdown::LayoutRun& run : line.runs) {
                const nmarkdown::Fx width = run.math != nullptr
                                                ? run.math->metrics.width
                                                : run.glyphs.width;
                CHECK(run.x >= previous_right);
                previous_right = run.x + width;
                saw_inline_math = saw_inline_math || run.math != nullptr;
            }
            CHECK(line.width >= previous_right);
            CHECK(line.advance >= line.ascent + line.descent +
                                      nmarkdown::fx_from_int(2));
        }
        CHECK(saw_inline_math);
    }
}

void test_phase3_sample_math() {
    nmarkdown::StdioFileSystem files;
    std::vector<std::uint8_t> bytes;
    std::string error;
    const std::string path = std::string(NMARKDOWN_SOURCE_DIR) + "/samples/phase2.md";
    CHECK(files.read_all(path.c_str(), 1024U * 1024U, bytes, error));
    nmarkdown::MarkdownDocument document;
    CHECK(nmarkdown::parse_markdown(bytes.data(), bytes.size(), document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(document, text, signature, error, &math));
    std::size_t formulas = 0;
    for (std::size_t index = 0; index < layout.unit_count(); ++index) {
        const nmarkdown::BlockLayout* block = layout.layout_unit(index);
        if (block == nullptr) continue;
        for (const nmarkdown::LayoutLine& line : block->lines) {
            for (const nmarkdown::LayoutRun& run : line.runs) {
                if (run.math == nullptr) continue;
                ++formulas;
                if (!run.math->valid) {
                    std::fprintf(stderr, "sample formula error: %s\n",
                                 run.math->diagnostic.c_str());
                }
                CHECK(run.math->valid);
                for (const nmarkdown::MathRule& rule : run.math->rules) {
                    if (rule.width > run.math->metrics.width + nmarkdown::fx_from_int(2) ||
                        (rule.height > nmarkdown::fx_from_int(4) &&
                         rule.width > nmarkdown::fx_from_int(1)) ||
                        rule.y < -run.math->metrics.ascent - nmarkdown::fx_from_int(4) ||
                        rule.y + rule.height > run.math->metrics.descent +
                                                     nmarkdown::fx_from_int(4)) {
                        std::fprintf(stderr,
                                     "bad rule w=%d h=%d y=%d box=%d/%d/%d\n",
                                     rule.width, rule.height, rule.y,
                                     run.math->metrics.width,
                                     run.math->metrics.ascent,
                                     run.math->metrics.descent);
                    }
                    CHECK(rule.width <= run.math->metrics.width + nmarkdown::fx_from_int(2));
                    CHECK(rule.height <= nmarkdown::fx_from_int(4) ||
                          rule.width <= nmarkdown::fx_from_int(1));
                }
            }
        }
    }
    CHECK(formulas >= 3);
}

void test_responsive_and_grid_tables() {
    const std::string source =
        "| Name | Value | Notes |\n"
        "|:-----|------:|:------|\n"
        "| alpha | 12345 | a deliberately long table value |\n";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    signature.content_width = 180;
    CHECK(layout.initialize(document, text, signature, error));
    CHECK(layout.unit_count() == 1);  // Header becomes labels in responsive mode.
    const nmarkdown::BlockLayout* responsive = layout.layout_unit(0);
    CHECK(responsive != nullptr);
    CHECK(responsive != nullptr && responsive->kind == nmarkdown::BlockKind::TableRow);
    CHECK(responsive != nullptr && responsive->horizontal_inset_px == 2);
    CHECK(responsive != nullptr && responsive->lines.size() >= 3);

    signature.table_mode = 1;
    CHECK(layout.reconfigure(signature, error));
    CHECK(layout.unit_count() == 2);  // Header and body are both grid rows.
    const nmarkdown::BlockLayout* header = layout.layout_unit(0);
    const nmarkdown::BlockLayout* body = layout.layout_unit(1);
    CHECK(header != nullptr && header->lines.size() == 1);
    CHECK(body != nullptr && body->lines.size() == 1);
    CHECK(body != nullptr && body->maximum_line_width > nmarkdown::fx_from_int(180));
}

void test_typographic_wrap_boundaries() {
    const std::string source =
        "alpha beta gamma delta epsilon zeta eta theta\n\n"
        "abcdefghij,klmnopqrstuvwxyz\n";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    signature.content_width = 86;
    CHECK(layout.initialize(document, text, signature, error));

    for (std::size_t block_index = 0; block_index < layout.unit_count(); ++block_index) {
        const nmarkdown::BlockLayout* block = layout.layout_unit(block_index);
        CHECK(block != nullptr);
        if (block == nullptr) continue;
        for (const nmarkdown::LayoutLine& line : block->lines) {
            if (line.runs.empty()) continue;
            const nmarkdown::LayoutRun& last = line.runs.back();
            CHECK(!last.glyphs.glyphs.empty());
            if (!last.glyphs.glyphs.empty()) {
                CHECK(last.glyphs.glyphs.back().codepoint != ' ');
            }
            const nmarkdown::LayoutRun& first = line.runs.front();
            if (first.source_length != 0 && first.source_offset < source.size()) {
                CHECK(source[first.source_offset] != ',');
            }
        }
    }
}

void test_configured_content_width_has_no_hidden_right_gutter() {
    const std::string source = "WWWWWWWWWWWWWWWWWWWW";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::GlyphRun natural;
    CHECK(text.shape(source.data(), source.size(), nmarkdown::fx_from_int(15),
                     natural));

    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    signature.content_width =
        static_cast<std::uint16_t>(nmarkdown::fx_ceil(natural.width));
    CHECK(layout.initialize(document, text, signature, error));
    const nmarkdown::BlockLayout* block = layout.layout_unit(0);
    CHECK(block != nullptr);
    CHECK(block != nullptr && block->horizontal_inset_px == 0);
    CHECK(block != nullptr && block->lines.size() == 1);
    if (block != nullptr && !block->lines.empty()) {
        const nmarkdown::LayoutLine& line = block->lines.front();
        CHECK(!line.runs.empty());
        CHECK(line.runs.empty() || line.runs.front().x == 0);
        CHECK(line.width <= nmarkdown::fx_from_int(signature.content_width));
        CHECK(line.width >
              nmarkdown::fx_from_int(signature.content_width - 4));
    }
}

void test_bounded_interword_shrink() {
    const std::string source = "alpha beta gamma";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::GlyphRun natural;
    CHECK(text.shape(source.data(), source.size(), nmarkdown::fx_from_int(15), natural));

    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    // Make the configured prose width just under the natural OpenType width:
    // bounded inter-word shrink should keep the final word without forcing a
    // wrap. There is no additional hidden right-side reserve.
    signature.content_width = static_cast<std::uint16_t>(
        nmarkdown::fx_ceil(natural.width) - 1);
    CHECK(layout.initialize(document, text, signature, error));
    const nmarkdown::BlockLayout* block = layout.layout_unit(0);
    CHECK(block != nullptr);
    CHECK(block != nullptr && block->lines.size() == 1);
    if (block != nullptr && !block->lines.empty()) {
        const nmarkdown::Fx usable =
            nmarkdown::fx_from_int(signature.content_width);
        CHECK(block->lines.front().width <= usable);
        CHECK(std::count_if(block->lines.front().runs.begin(),
                            block->lines.front().runs.end(),
                            [](const nmarkdown::LayoutRun& run) {
                                return run.collapsible_space;
                            }) == 2);
    }
}

void test_code_blocks_wrap_by_default_and_pan_is_explicit() {
    const std::string source =
        "```cpp\n"
        "const char* text = \"this source-code line is deliberately much wider than the calculator viewport\";\n"
        "```\n";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    signature.content_width = 120;
    CHECK(signature.code_wrap == 1);
    CHECK(layout.initialize(document, text, signature, error));

    const nmarkdown::BlockLayout* wrapped = layout.layout_unit(0);
    CHECK(wrapped != nullptr && wrapped->code_background);
    CHECK(wrapped != nullptr && wrapped->horizontal_inset_px == 6);
    CHECK(wrapped != nullptr && wrapped->lines.size() > 1);
    CHECK(wrapped != nullptr &&
          wrapped->maximum_line_width <=
              nmarkdown::fx_from_int(signature.content_width - 12));

    nmarkdown::BlockLayout focused;
    CHECK(layout.layout_unwrapped_code_unit(0, focused));
    CHECK(focused.code_background);
    CHECK(focused.lines.size() == 1);
    CHECK(focused.maximum_line_width >
          nmarkdown::fx_from_int(signature.content_width));
    // The alternate focus canvas must not mutate the wrapped document layout
    // or its measured vertical geometry.
    const nmarkdown::BlockLayout* still_wrapped = layout.layout_unit(0);
    CHECK(still_wrapped != nullptr && still_wrapped->lines.size() > 1);
    CHECK(still_wrapped != nullptr &&
          still_wrapped->maximum_line_width <=
              nmarkdown::fx_from_int(signature.content_width - 12));

    signature.code_wrap = 0;
    CHECK(layout.reconfigure(signature, error));
    const nmarkdown::BlockLayout* pannable = layout.layout_unit(0);
    CHECK(pannable != nullptr && pannable->lines.size() == 1);
    CHECK(pannable != nullptr &&
          pannable->maximum_line_width > nmarkdown::fx_from_int(signature.content_width));
}

void test_format_gallery_layout() {
    nmarkdown::StdioFileSystem files;
    std::vector<std::uint8_t> bytes;
    std::string error;
    const std::string path = std::string(NMARKDOWN_SOURCE_DIR) +
                             "/samples/format-gallery.md";
    CHECK(files.read_all(path.c_str(), 1024U * 1024U, bytes, error));
    nmarkdown::MarkdownDocument document;
    CHECK(nmarkdown::parse_markdown(bytes.data(), bytes.size(), document, error));
    CHECK(document.source.find(u8"中文排版测试") != std::string::npos);
    CHECK(document.source.find(u8"日本語かなカナ") != std::string::npos);

    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    std::vector<std::uint8_t> body;
    std::vector<std::uint8_t> italic;
    std::vector<std::uint8_t> monospace;
    CHECK(read_font_asset("DejaVuSans-CX.ttf", body));
    CHECK(read_font_asset("DejaVuSans-Oblique-CX.ttf", italic));
    CHECK(read_font_asset("DejaVuSansMono-CX.ttf", monospace));
    CHECK(text.set_external_font(nmarkdown::FontRole::BodySans,
                                 body.data(), body.size(), error));
    CHECK(text.set_external_font(nmarkdown::FontRole::BodySansItalic,
                                 italic.data(), italic.size(), error));
    CHECK(text.set_external_font(nmarkdown::FontRole::Monospace,
                                 monospace.data(), monospace.size(), error));
    nmarkdown::MathSystem math(text);
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(document, text, signature, error, &math));

    std::size_t headings = 0;
    std::size_t code_blocks = 0;
    std::size_t table_rows = 0;
    std::size_t quoted_blocks = 0;
    std::size_t inline_formulas = 0;
    std::size_t display_formulas = 0;
    bool strong = false;
    bool emphasis = false;
    bool native_italic_face = false;
    bool monospace_face = false;
    for (std::size_t index = 0; index < layout.unit_count(); ++index) {
        const nmarkdown::BlockLayout* block = layout.layout_unit(index);
        CHECK(block != nullptr);
        if (block == nullptr) continue;
        headings += block->kind == nmarkdown::BlockKind::Heading ? 1 : 0;
        code_blocks += block->code_background ? 1 : 0;
        table_rows += block->kind == nmarkdown::BlockKind::TableRow ? 1 : 0;
        quoted_blocks += block->quote_depth != 0 ? 1 : 0;
        for (const nmarkdown::LayoutLine& line : block->lines) {
            for (const nmarkdown::LayoutRun& run : line.runs) {
                strong = strong || (run.style_flags & nmarkdown::InlineStyleStrong) != 0;
                emphasis = emphasis ||
                           (run.style_flags & nmarkdown::InlineStyleEmphasis) != 0;
                if ((run.style_flags & nmarkdown::InlineStyleEmphasis) != 0) {
                    native_italic_face = native_italic_face ||
                        std::any_of(run.glyphs.glyphs.begin(),
                                    run.glyphs.glyphs.end(),
                                    [](const nmarkdown::PositionedGlyph& glyph) {
                                        return glyph.face == 2002;
                                    });
                }
                if (run.math != nullptr) {
                    inline_formulas += run.display_math ? 0 : 1;
                    display_formulas += run.display_math ? 1 : 0;
                }
                if (block->code_background ||
                    (run.style_flags & nmarkdown::InlineStyleCode) != 0) {
                    for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                        monospace_face = monospace_face || glyph.face == 2003;
                    }
                }
            }
        }
    }
    CHECK(headings >= 5);
    CHECK(code_blocks == 1);
    CHECK(table_rows >= 4);
    CHECK(quoted_blocks >= 1);
    CHECK(inline_formulas >= 3);
    CHECK(display_formulas >= 2);
    CHECK(strong && emphasis && native_italic_face && monospace_face);
}

void test_idle_preload_is_incremental_and_bounded() {
    std::string source = "# Prefetch probe\n\n";
    for (int index = 0; index < 600; ++index) {
        source += "Short future paragraph " + std::to_string(index) +
                  " stays inside the bounded idle layout window.\n\n";
    }

    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(document, text, signature, error));

    constexpr int kViewport = 220;
    const auto first = layout.layout_window(0, nmarkdown::fx_from_int(kViewport), 0);
    CHECK(!first.empty());
    const std::size_t visible_measured = layout.measured_count();
    CHECK(visible_measured > 0);

    const std::size_t before_one_step = layout.measured_count();
    CHECK(layout.preload_next(nmarkdown::fx_from_int(kViewport),
                              nmarkdown::fx_from_int(kViewport * 3)));
    CHECK(layout.measured_count() <= before_one_step + 1);

    int idle_steps = 1;
    while (idle_steps < 256 &&
           layout.preload_next(nmarkdown::fx_from_int(kViewport),
                               nmarkdown::fx_from_int(kViewport * 3))) {
        ++idle_steps;
    }
    CHECK(idle_steps > 1);
    CHECK(idle_steps < 96);
    CHECK(layout.measured_count() > visible_measured);
    CHECK(layout.measured_count() < layout.unit_count());
    CHECK(layout.cache_size() <= 96);

    // The immediately following page is already measured by idle work, so a
    // page turn does not synchronously grow the measured set.
    const std::size_t warmed = layout.measured_count();
    const auto next = layout.layout_window(nmarkdown::fx_from_int(kViewport),
                                           nmarkdown::fx_from_int(kViewport), 0);
    CHECK(!next.empty());
    CHECK(layout.measured_count() == warmed);

    // The renderer-facing look-ahead advances one shaped line per call and
    // can rasterize it without allocating a page framebuffer.
    nmarkdown::VirtualDocumentLayout line_layout;
    CHECK(line_layout.initialize(document, text, signature, error));
    CHECK(!line_layout.layout_window(0, nmarkdown::fx_from_int(kViewport), 0).empty());
    const std::size_t before_line = line_layout.measured_count();
    const nmarkdown::LayoutLine* future_line = line_layout.preload_next_line(
        nmarkdown::fx_from_int(kViewport),
        nmarkdown::fx_from_int(kViewport * 3));
    CHECK(future_line != nullptr);
    CHECK(line_layout.measured_count() <= before_line + 1);
    if (future_line != nullptr && !future_line->runs.empty()) {
        const nmarkdown::LayoutRun& run = future_line->runs.front();
        const nmarkdown::GlyphCacheStats before = text.cache_stats();
        CHECK(text.cache_run(run.glyphs, run.pixel_size, 48) > 0);
        const nmarkdown::GlyphCacheStats after = text.cache_stats();
        CHECK(after.hits + after.misses > before.hits + before.misses);
    }
}

void test_plain_text_chunk_boundaries_are_visually_continuous() {
    std::string source;
    for (int line = 0; line < 9; ++line) {
        source += "physical line " + std::to_string(line) + "\n";
    }
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_plain_text(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    CHECK(document.ir.blocks.size() == 2);
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(document, text, signature, error));
    const nmarkdown::BlockLayout* first = layout.layout_unit(0);
    const nmarkdown::BlockLayout* second = layout.layout_unit(1);
    CHECK(first != nullptr && second != nullptr);
    if (first == nullptr || second == nullptr || first->lines.empty() ||
        second->lines.empty()) {
        return;
    }
    CHECK(first->lines.size() == 8);
    CHECK(second->lines.size() == 1);
    const nmarkdown::LayoutLine& last_before_boundary = first->lines.back();
    const nmarkdown::LayoutLine& first_after_boundary = second->lines.front();
    const nmarkdown::Fx before_top = layout.unit_top(0) +
        last_before_boundary.baseline_y - last_before_boundary.ascent;
    const nmarkdown::Fx after_top = layout.unit_top(1) +
        first_after_boundary.baseline_y - first_after_boundary.ascent;
    CHECK(after_top - before_top == last_before_boundary.advance);
}

void test_task_lists_use_native_checkbox_runs() {
    const std::string source = "- [x] finished\n- [ ] pending\n";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(document, text, signature, error));
    CHECK(layout.unit_count() == 2);

    for (std::size_t index = 0; index < layout.unit_count() && index < 2;
         ++index) {
        const nmarkdown::BlockLayout* block = layout.layout_unit(index);
        CHECK(block != nullptr && !block->lines.empty() &&
              !block->lines.front().runs.empty());
        if (block == nullptr || block->lines.empty() ||
            block->lines.front().runs.empty()) {
            continue;
        }
        const nmarkdown::LayoutRun& checkbox =
            block->lines.front().runs.front();
        CHECK(checkbox.task_checkbox);
        CHECK(checkbox.task_checked == (index == 0));
        CHECK(checkbox.glyphs.glyphs.empty());
        CHECK(checkbox.glyphs.width > checkbox.glyphs.ascent);
    }
}

// A synthetic list prefix ("7. ", "• ") is shaped from text that does not
// exist in the source, so its run carries no source mapping. Identify it by
// comparing shaped glyph ids against the expected prefix string.
bool first_run_is_prefix(const nmarkdown::BlockLayout* block,
                         nmarkdown::TextSystem& text,
                         const std::string& prefix) {
    if (block == nullptr || block->lines.empty() ||
        block->lines.front().runs.empty()) {
        return false;
    }
    const nmarkdown::LayoutRun& run = block->lines.front().runs.front();
    if (run.task_checkbox || run.exact_source_mapping) {
        return false;
    }
    // The line builder appends the marker word and defers its trailing space
    // into a separate collapsible run, so compare against the trimmed word.
    std::string word = prefix;
    while (!word.empty() && word.back() == ' ') word.pop_back();
    nmarkdown::GlyphRun expected;
    if (!text.shape(word.data(), word.size(), nmarkdown::fx_from_int(15),
                    expected, nmarkdown::FontRole::BodySans,
                    nmarkdown::TextSpacing::Natural,
                    nmarkdown::FontStyle::Bold)) {
        return false;
    }
    if (expected.glyphs.size() != run.glyphs.glyphs.size()) return false;
    for (std::size_t index = 0; index < expected.glyphs.size(); ++index) {
        if (expected.glyphs[index].glyph != run.glyphs.glyphs[index].glyph) {
            return false;
        }
    }
    return true;
}

bool first_run_is_document_text(const nmarkdown::BlockLayout* block) {
    return block != nullptr && !block->lines.empty() &&
           !block->lines.front().runs.empty() &&
           block->lines.front().runs.front().exact_source_mapping;
}

// Golden semantics for list prefixes: only the first unit belonging to a list
// item carries the marker; a paragraph following a nested sub-list inside the
// same outer item does not restart the marker; ordered numbering follows
// sibling position seeded by the list's start value; a later list restarts.
void test_list_prefixes_match_document_order() {
    const std::string source =
        "7. seven first paragraph\n"
        "\n"
        "   seven second paragraph\n"
        "\n"
        "8. eight first\n"
        "   1. nested one\n"
        "   2. nested two\n"
        "\n"
        "   eight after nested\n"
        "\n"
        "- bullet one\n"
        "\n"
        "  bullet one second\n"
        "- [x] done task\n"
        "- [ ] open task\n"
        "\n"
        "1. restart one\n";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        document, error));
    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::VirtualDocumentLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(document, text, signature, error));
    CHECK(layout.unit_count() == 11);
    if (layout.unit_count() != 11) return;

    CHECK(first_run_is_prefix(layout.layout_unit(0), text, "7. "));
    CHECK(first_run_is_document_text(layout.layout_unit(1)));
    CHECK(first_run_is_prefix(layout.layout_unit(2), text, "8. "));
    CHECK(first_run_is_prefix(layout.layout_unit(3), text, "1. "));
    CHECK(first_run_is_prefix(layout.layout_unit(4), text, "2. "));
    CHECK(first_run_is_document_text(layout.layout_unit(5)));
    CHECK(first_run_is_prefix(layout.layout_unit(6), text, u8"• "));
    CHECK(first_run_is_document_text(layout.layout_unit(7)));
    const nmarkdown::BlockLayout* done = layout.layout_unit(8);
    CHECK(done != nullptr && !done->lines.empty() &&
          !done->lines.front().runs.empty() &&
          done->lines.front().runs.front().task_checkbox &&
          done->lines.front().runs.front().task_checked);
    const nmarkdown::BlockLayout* open = layout.layout_unit(9);
    CHECK(open != nullptr && !open->lines.empty() &&
          !open->lines.front().runs.empty() &&
          open->lines.front().runs.front().task_checkbox &&
          !open->lines.front().runs.front().task_checked);
    CHECK(first_run_is_prefix(layout.layout_unit(10), text, "1. "));

    // Long flat ordered list: numbering must track sibling position for
    // every item, not just early ones.
    std::string flat = "# Flat\n\n";
    for (int item = 1; item <= 600; ++item) {
        flat += std::to_string(item) + ". item " + std::to_string(item) +
                "\n";
    }
    nmarkdown::MarkdownDocument flat_document;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(flat.data()), flat.size(),
        flat_document, error));
    nmarkdown::VirtualDocumentLayout flat_layout;
    CHECK(flat_layout.initialize(flat_document, text, signature, error));
    CHECK(flat_layout.unit_count() == 601);
    if (flat_layout.unit_count() != 601) return;
    CHECK(first_run_is_prefix(flat_layout.layout_unit(1), text, "1. "));
    CHECK(first_run_is_prefix(flat_layout.layout_unit(300), text, "300. "));
    CHECK(first_run_is_prefix(flat_layout.layout_unit(600), text, "600. "));
}

}  // namespace

int main() {
    test_lazy_layout_and_anchor();
    test_inline_and_display_math_integration();
    test_automatic_content_aware_line_spacing();
    test_inline_math_never_overlaps_following_prose();
    test_phase3_sample_math();
    test_responsive_and_grid_tables();
    test_typographic_wrap_boundaries();
    test_configured_content_width_has_no_hidden_right_gutter();
    test_bounded_interword_shrink();
    test_code_blocks_wrap_by_default_and_pan_is_explicit();
    test_format_gallery_layout();
    test_idle_preload_is_incremental_and_bounded();
    test_plain_text_chunk_boundaries_are_visually_continuous();
    test_task_lists_use_native_checkbox_runs();
    test_list_prefixes_match_document_order();
    if (failures != 0) {
        std::fprintf(stderr, "%d block layout test(s) failed\n", failures);
        return 1;
    }
    std::printf("All block layout tests passed\n");
    return 0;
}
