#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "nmarkdown/math/math_system.h"
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

using nmarkdown::AtomClass;

struct ExpectedSymbol {
    const char* name;
    const char* text;
    AtomClass atom_class;
    bool large_glyph;
    bool movable_limits;
};

constexpr ExpectedSymbol kExpectedSymbols[] = {
#include "../src/math/math_symbol_table.inc"
};

void test_layout_and_cache() {
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    std::shared_ptr<const nmarkdown::MathLayoutResult> fraction;
    CHECK(math.layout("\\frac{x_i^2}{\\sqrt[n]{y}}",
                      nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(18),
                      nmarkdown::fx_from_int(280), fraction));
    CHECK(fraction != nullptr);
    CHECK(fraction->valid);
    CHECK(fraction->metrics.width > 0);
    CHECK(fraction->metrics.ascent > nmarkdown::fx_from_int(10));
    CHECK(fraction->metrics.descent > 0);
    CHECK(!fraction->rules.empty());

    std::shared_ptr<const nmarkdown::MathLayoutResult> cached;
    CHECK(math.layout("\\frac{x_i^2}{\\sqrt[n]{y}}",
                      nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(18),
                      nmarkdown::fx_from_int(280), cached));
    CHECK(cached.get() == fraction.get());
    CHECK(math.cache_stats().hits == 1);

    std::shared_ptr<const nmarkdown::MathLayoutResult> quadratic;
    CHECK(math.layout("\\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}",
                      nmarkdown::MathStyle::Text,
                      nmarkdown::fx_from_int(15),
                      nmarkdown::fx_from_int(280), quadratic));
    CHECK(quadratic != nullptr);
    CHECK(quadratic->valid);

    std::shared_ptr<const nmarkdown::MathLayoutResult> plus_sign;
    std::shared_ptr<const nmarkdown::MathLayoutResult> minus_sign;
    CHECK(math.layout("+", nmarkdown::MathStyle::Text,
                      nmarkdown::fx_from_int(15),
                      nmarkdown::fx_from_int(280), plus_sign));
    CHECK(math.layout("-", nmarkdown::MathStyle::Text,
                      nmarkdown::fx_from_int(15),
                      nmarkdown::fx_from_int(280), minus_sign));
    CHECK(plus_sign != nullptr && minus_sign != nullptr);
    if (plus_sign != nullptr && minus_sign != nullptr) {
        CHECK(plus_sign->metrics.width == minus_sign->metrics.width);
        CHECK(!plus_sign->runs.empty() && !minus_sign->runs.empty());
        if (!plus_sign->runs.empty() && !minus_sign->runs.empty()) {
            CHECK(!plus_sign->runs.front().glyphs.glyphs.empty());
            CHECK(!minus_sign->runs.front().glyphs.glyphs.empty());
            if (!plus_sign->runs.front().glyphs.glyphs.empty() &&
                !minus_sign->runs.front().glyphs.glyphs.empty()) {
                CHECK(plus_sign->runs.front().glyphs.glyphs.front().x_advance ==
                      minus_sign->runs.front().glyphs.glyphs.front().x_advance);
                CHECK(minus_sign->runs.front().glyphs.glyphs.front().codepoint ==
                      0x2212U);
            }
        }
    }

    struct AtomGeometry {
        nmarkdown::Fx x = 0;
        nmarkdown::Fx right = 0;
        nmarkdown::Fx baseline = 0;
        bool found = false;
    };
    const auto atom_geometry = [](const nmarkdown::MathLayoutResult& result,
                                  std::uint32_t codepoint) {
        AtomGeometry geometry;
        for (const nmarkdown::MathDrawRun& run : result.runs) {
            if (run.glyphs.glyphs.empty() ||
                run.glyphs.glyphs.front().codepoint != codepoint) {
                continue;
            }
            geometry = {run.x, run.x + run.glyphs.width,
                        run.baseline_y, true};
            break;
        }
        return geometry;
    };
    const auto layout_spacing_case = [&](const char* source) {
        std::shared_ptr<const nmarkdown::MathLayoutResult> result;
        CHECK(math.layout(source, nmarkdown::MathStyle::Text,
                          nmarkdown::fx_from_int(16),
                          nmarkdown::fx_from_int(280), result));
        CHECK(result != nullptr && result->valid);
        return result;
    };
    const nmarkdown::Fx thin_mu =
        nmarkdown::fx_from_int(16) * 3 / 18;
    const nmarkdown::Fx medium_mu =
        nmarkdown::fx_from_int(16) * 4 / 18;
    const nmarkdown::Fx thick_mu =
        nmarkdown::fx_from_int(16) * 5 / 18;
    const nmarkdown::Fx control_space_mu =
        nmarkdown::fx_from_int(16) * 6 / 18;

    const auto binary_spacing = layout_spacing_case("a-b");
    const AtomGeometry binary_a = atom_geometry(*binary_spacing, 0x1D44EU);
    const AtomGeometry binary_minus = atom_geometry(*binary_spacing, 0x2212U);
    const AtomGeometry binary_b = atom_geometry(*binary_spacing, 0x1D44FU);
    CHECK(binary_a.found && binary_minus.found && binary_b.found);
    CHECK(binary_minus.x - binary_a.right == medium_mu);
    CHECK(binary_b.x - binary_minus.right == medium_mu);

    const auto unary_spacing = layout_spacing_case("a=-b");
    const AtomGeometry unary_a = atom_geometry(*unary_spacing, 0x1D44EU);
    const AtomGeometry unary_equal = atom_geometry(*unary_spacing, '=');
    const AtomGeometry unary_minus = atom_geometry(*unary_spacing, 0x2212U);
    const AtomGeometry unary_b = atom_geometry(*unary_spacing, 0x1D44FU);
    CHECK(unary_a.found && unary_equal.found && unary_minus.found &&
          unary_b.found);
    CHECK(unary_equal.x - unary_a.right == thick_mu);
    CHECK(unary_minus.x - unary_equal.right == thick_mu);
    CHECK(unary_b.x == unary_minus.right);

    const auto spaced_unary = layout_spacing_case(R"(a=\!-b)");
    const AtomGeometry spaced_equal = atom_geometry(*spaced_unary, '=');
    const AtomGeometry spaced_minus = atom_geometry(*spaced_unary, 0x2212U);
    const AtomGeometry spaced_b = atom_geometry(*spaced_unary, 0x1D44FU);
    CHECK(spaced_equal.found && spaced_minus.found && spaced_b.found);
    CHECK(spaced_minus.x - spaced_equal.right == thick_mu - thin_mu);
    CHECK(spaced_b.x == spaced_minus.right);

    const auto control_spaced_unary = layout_spacing_case("a=\\ -b");
    const AtomGeometry control_equal =
        atom_geometry(*control_spaced_unary, '=');
    const AtomGeometry control_minus =
        atom_geometry(*control_spaced_unary, 0x2212U);
    const AtomGeometry control_b =
        atom_geometry(*control_spaced_unary, 0x1D44FU);
    CHECK(control_equal.found && control_minus.found && control_b.found);
    CHECK(control_minus.x - control_equal.right ==
          thick_mu + control_space_mu);
    CHECK(control_b.x == control_minus.right);

    const auto parenthesized_unary = layout_spacing_case("(-x)");
    const AtomGeometry opening = atom_geometry(*parenthesized_unary, '(');
    const AtomGeometry parenthesized_minus =
        atom_geometry(*parenthesized_unary, 0x2212U);
    const AtomGeometry parenthesized_x =
        atom_geometry(*parenthesized_unary, 0x1D465U);
    const AtomGeometry closing = atom_geometry(*parenthesized_unary, ')');
    CHECK(opening.found && parenthesized_minus.found &&
          parenthesized_x.found && closing.found);
    CHECK(parenthesized_minus.x == opening.right);
    CHECK(parenthesized_x.x == parenthesized_minus.right);
    CHECK(closing.x == parenthesized_x.right);

    const auto leading_unary = layout_spacing_case("-x");
    const AtomGeometry leading_unary_minus =
        atom_geometry(*leading_unary, 0x2212U);
    const AtomGeometry leading_unary_x =
        atom_geometry(*leading_unary, 0x1D465U);
    CHECK(leading_unary_minus.found && leading_unary_x.found);
    CHECK(leading_unary_minus.x == 0);
    CHECK(leading_unary_x.x == leading_unary_minus.right);

    // TeX users intentionally write an empty Ordinary group to prevent a
    // following binary operator from being demoted to unary.
    const auto forced_binary = layout_spacing_case("{}-x");
    const AtomGeometry forced_binary_minus =
        atom_geometry(*forced_binary, 0x2212U);
    const AtomGeometry forced_binary_x =
        atom_geometry(*forced_binary, 0x1D465U);
    CHECK(forced_binary_minus.found && forced_binary_x.found);
    CHECK(forced_binary_minus.x == medium_mu);
    CHECK(forced_binary_x.x - forced_binary_minus.right == medium_mu);

    const auto trailing_binary = layout_spacing_case("a+");
    const AtomGeometry trailing_a = atom_geometry(*trailing_binary, 0x1D44EU);
    const AtomGeometry trailing_plus = atom_geometry(*trailing_binary, '+');
    CHECK(trailing_a.found && trailing_plus.found);
    CHECK(trailing_plus.x == trailing_a.right);

    const auto operator_spacing = layout_spacing_case(R"(a\sin b)");
    const AtomGeometry operator_a = atom_geometry(*operator_spacing, 0x1D44EU);
    const AtomGeometry sin_run = atom_geometry(*operator_spacing, 's');
    const AtomGeometry operator_b = atom_geometry(*operator_spacing, 0x1D44FU);
    CHECK(operator_a.found && sin_run.found && operator_b.found);
    CHECK(sin_run.x - operator_a.right == thin_mu);
    CHECK(operator_b.x - sin_run.right == thin_mu);

    const auto inner_spacing = layout_spacing_case(R"(a\cdots b)");
    const AtomGeometry inner_a = atom_geometry(*inner_spacing, 0x1D44EU);
    const AtomGeometry inner_dots = atom_geometry(*inner_spacing, 0x22EFU);
    const AtomGeometry inner_b = atom_geometry(*inner_spacing, 0x1D44FU);
    CHECK(inner_a.found && inner_dots.found && inner_b.found);
    CHECK(inner_dots.x - inner_a.right == thin_mu);
    CHECK(inner_b.x - inner_dots.right == thin_mu);

    const auto grouped_spacing = layout_spacing_case("a{b}");
    const AtomGeometry grouped_a = atom_geometry(*grouped_spacing, 0x1D44EU);
    const AtomGeometry grouped_b = atom_geometry(*grouped_spacing, 0x1D44FU);
    CHECK(grouped_a.found && grouped_b.found);
    CHECK(grouped_b.x == grouped_a.right);

    const auto styled_spacing = layout_spacing_case(R"(a\mathbf{b})");
    const AtomGeometry styled_a = atom_geometry(*styled_spacing, 0x1D44EU);
    const AtomGeometry styled_b = atom_geometry(*styled_spacing, 0x1D41BU);
    CHECK(styled_a.found && styled_b.found);
    CHECK(styled_b.x == styled_a.right);

    const auto standalone_radical = layout_spacing_case(R"(\sqrt{x})");
    const auto radical_spacing = layout_spacing_case(R"(2\sqrt{x}y)");
    const AtomGeometry radical_two = atom_geometry(*radical_spacing, '2');
    const AtomGeometry radical_sign = atom_geometry(*radical_spacing, 0x221AU);
    const AtomGeometry radical_y = atom_geometry(*radical_spacing, 0x1D466U);
    CHECK(radical_two.found && radical_sign.found && radical_y.found);
    CHECK(radical_sign.x == radical_two.right);
    CHECK(radical_y.x == radical_sign.x + standalone_radical->metrics.width);

    const auto standalone_fraction = layout_spacing_case(R"(\frac{1}{2})");
    const auto fraction_spacing = layout_spacing_case(R"(a\frac{1}{2}b)");
    const AtomGeometry fraction_a = atom_geometry(*fraction_spacing, 0x1D44EU);
    const AtomGeometry fraction_one = atom_geometry(*fraction_spacing, '1');
    const AtomGeometry fraction_b = atom_geometry(*fraction_spacing, 0x1D44FU);
    CHECK(fraction_a.found && fraction_one.found && fraction_b.found);
    CHECK(fraction_one.x - fraction_a.right >= thin_mu);
    CHECK(fraction_b.x == fraction_a.right + thin_mu +
                           standalone_fraction->metrics.width + thin_mu);

    const auto verify_script_style = [&](nmarkdown::MathStyle outer_style) {
        std::shared_ptr<const nmarkdown::MathLayoutResult> result;
        CHECK(math.layout(R"(x^{a+b})", outer_style,
                          nmarkdown::fx_from_int(16),
                          nmarkdown::fx_from_int(280), result));
        CHECK(result != nullptr && result->valid);
        if (result == nullptr) return;
        const nmarkdown::MathDrawRun* a_run = nullptr;
        const nmarkdown::MathDrawRun* plus_run = nullptr;
        const nmarkdown::MathDrawRun* b_run = nullptr;
        for (const nmarkdown::MathDrawRun& run : result->runs) {
            if (run.glyphs.glyphs.empty()) continue;
            const std::uint32_t codepoint =
                run.glyphs.glyphs.front().codepoint;
            if (codepoint == 0x1D44EU) a_run = &run;
            if (codepoint == '+') plus_run = &run;
            if (codepoint == 0x1D44FU) b_run = &run;
        }
        CHECK(a_run != nullptr && plus_run != nullptr && b_run != nullptr);
        if (a_run == nullptr || plus_run == nullptr || b_run == nullptr) return;
        CHECK(a_run->pixel_size == nmarkdown::fx_from_int(12));
        CHECK(plus_run->pixel_size == nmarkdown::fx_from_int(12));
        CHECK(b_run->pixel_size == nmarkdown::fx_from_int(12));
        CHECK(plus_run->x == a_run->x + a_run->glyphs.width);
        CHECK(b_run->x == plus_run->x + plus_run->glyphs.width);
    };
    verify_script_style(nmarkdown::MathStyle::Display);
    verify_script_style(nmarkdown::MathStyle::Text);

    std::shared_ptr<const nmarkdown::MathLayoutResult> display_fraction_style;
    std::shared_ptr<const nmarkdown::MathLayoutResult> text_fraction_style;
    CHECK(math.layout(R"(\frac{a}{b})", nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16), nmarkdown::fx_from_int(280),
                      display_fraction_style));
    CHECK(math.layout(R"(\frac{a}{b})", nmarkdown::MathStyle::Text,
                      nmarkdown::fx_from_int(16), nmarkdown::fx_from_int(280),
                      text_fraction_style));
    if (display_fraction_style != nullptr && text_fraction_style != nullptr) {
        CHECK(!display_fraction_style->runs.empty());
        CHECK(!text_fraction_style->runs.empty());
        if (!display_fraction_style->runs.empty() &&
            !text_fraction_style->runs.empty()) {
            CHECK(display_fraction_style->runs.front().pixel_size ==
                  nmarkdown::fx_from_int(16));
            CHECK(text_fraction_style->runs.front().pixel_size ==
                  nmarkdown::fx_from_int(12));
        }
    }

    const auto check_script_placement = [&](const char* source,
                                            std::uint32_t base_codepoint,
                                            bool stacked) {
        std::shared_ptr<const nmarkdown::MathLayoutResult> result;
        CHECK(math.layout(source, nmarkdown::MathStyle::Display,
                          nmarkdown::fx_from_int(16),
                          nmarkdown::fx_from_int(280), result));
        CHECK(result != nullptr && result->valid);
        if (result == nullptr) return;
        const AtomGeometry base = atom_geometry(*result, base_codepoint);
        const AtomGeometry sub = atom_geometry(*result, '0');
        const AtomGeometry sup = atom_geometry(*result, '1');
        CHECK(base.found && sub.found && sup.found);
        if (!base.found || !sub.found || !sup.found) return;
        CHECK(sup.baseline < base.baseline);
        CHECK(sub.baseline > base.baseline);
        if (stacked) {
            CHECK(sup.x < base.right);
            CHECK(sub.x < base.right);
        } else {
            CHECK(sup.x >= base.right);
            CHECK(sub.x >= base.right);
        }
    };
    check_script_placement(R"(\sum_0^1)", 0x2211U, true);
    check_script_placement(R"(\bm{\sum}_0^1)", 0x2211U, true);
    check_script_placement(R"(\int_0^1)", 0x222BU, false);
    check_script_placement(R"(\intop_0^1)", 0x222BU, true);
    check_script_placement(R"(\smallint_0^1)", 0x222BU, true);
    check_script_placement(R"(\lim_0^1)", 'l', true);

    std::shared_ptr<const nmarkdown::MathLayoutResult> base_arrows;
    CHECK(math.layout(R"(\uparrow+\Downarrow)", nmarkdown::MathStyle::Text,
                      nmarkdown::fx_from_int(16), nmarkdown::fx_from_int(280),
                      base_arrows));
    nmarkdown::GlyphId base_up_glyph = 0;
    nmarkdown::GlyphId base_down_glyph = 0;
    if (base_arrows != nullptr) {
        for (const nmarkdown::MathDrawRun& run : base_arrows->runs) {
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                if (glyph.codepoint == 0x2191U) base_up_glyph = glyph.glyph;
                if (glyph.codepoint == 0x21D3U) base_down_glyph = glyph.glyph;
            }
        }
    }
    CHECK(base_up_glyph != 0 && base_down_glyph != 0);

    std::shared_ptr<const nmarkdown::MathLayoutResult> tall_arrow_delimiters;
    CHECK(math.layout(
        R"(\left\uparrow\begin{matrix}a\\b\\c\end{matrix}\right\Downarrow)",
        nmarkdown::MathStyle::Display, nmarkdown::fx_from_int(16),
        nmarkdown::fx_from_int(280), tall_arrow_delimiters));
    bool found_native_tall_up = false;
    bool found_native_tall_down = false;
    if (tall_arrow_delimiters != nullptr) {
        for (const nmarkdown::MathDrawRun& run : tall_arrow_delimiters->runs) {
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                if (glyph.codepoint == 0x2191U) {
                    found_native_tall_up = glyph.face == 3 &&
                                           glyph.glyph != base_up_glyph;
                }
                if (glyph.codepoint == 0x21D3U) {
                    found_native_tall_down = glyph.face == 3 &&
                                             glyph.glyph != base_down_glyph;
                }
            }
        }
    }
    CHECK(found_native_tall_up);
    CHECK(found_native_tall_down);

    std::shared_ptr<const nmarkdown::MathLayoutResult> base_backslash;
    CHECK(math.layout(R"(\backslash)", nmarkdown::MathStyle::Text,
                      nmarkdown::fx_from_int(16), nmarkdown::fx_from_int(280),
                      base_backslash));
    nmarkdown::GlyphId base_backslash_glyph = 0;
    if (base_backslash != nullptr) {
        for (const nmarkdown::MathDrawRun& run : base_backslash->runs) {
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                if (glyph.codepoint == static_cast<std::uint32_t>('\\')) {
                    base_backslash_glyph = glyph.glyph;
                }
            }
        }
    }
    CHECK(base_backslash_glyph != 0);
    std::shared_ptr<const nmarkdown::MathLayoutResult> tall_backslash;
    CHECK(math.layout(
        R"(\left/\begin{matrix}a\\b\\c\end{matrix}\right\backslash)",
        nmarkdown::MathStyle::Display, nmarkdown::fx_from_int(16),
        nmarkdown::fx_from_int(280), tall_backslash));
    bool found_native_tall_backslash = false;
    if (tall_backslash != nullptr) {
        for (const nmarkdown::MathDrawRun& run : tall_backslash->runs) {
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                if (glyph.codepoint == static_cast<std::uint32_t>('\\')) {
                    found_native_tall_backslash = glyph.face == 3 &&
                        glyph.glyph != base_backslash_glyph;
                }
            }
        }
    }
    CHECK(found_native_tall_backslash);

    std::shared_ptr<const nmarkdown::MathLayoutResult> constructed_root;
    CHECK(math.layout("\\sqrt[3]{x+1}",
                      nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(280), constructed_root));
    CHECK(constructed_root != nullptr);
    bool found_radical_glyph = false;
    bool found_radical_overbar = false;
    bool found_latin_modern_radical = false;
    nmarkdown::Fx rightmost_radical_run = 0;
    nmarkdown::Fx radical_glyph_right = 0;
    nmarkdown::Fx radical_overbar_right = 0;
    for (const nmarkdown::MathDrawRun& run : constructed_root->runs) {
        rightmost_radical_run = std::max(rightmost_radical_run,
                                         run.x + run.glyphs.width);
        for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
            found_radical_glyph = found_radical_glyph ||
                                  glyph.codepoint == 0x221AU;
            found_latin_modern_radical = found_latin_modern_radical ||
                                         (glyph.codepoint == 0x221AU &&
                                          glyph.face == 3);
            if (glyph.codepoint == 0x221AU) {
                radical_glyph_right = std::max(radical_glyph_right,
                                                run.x + run.glyphs.width);
            }
        }
    }
    for (const nmarkdown::MathRule& rule : constructed_root->rules) {
        found_radical_overbar = found_radical_overbar ||
            (rule.width > nmarkdown::fx_from_int(2) &&
             rule.height == nmarkdown::fx_from_int(1));
        if (rule.width > nmarkdown::fx_from_int(2) &&
            rule.height == nmarkdown::fx_from_int(1)) {
            radical_overbar_right = std::max(radical_overbar_right,
                                              rule.x + rule.width);
        }
    }
    CHECK(found_radical_glyph);
    CHECK(found_latin_modern_radical);
    CHECK(found_radical_overbar);
    CHECK(radical_overbar_right >= rightmost_radical_run +
                                      nmarkdown::fx_from_int(2));

    // The extension rule must meet the Latin Modern radical glyph without
    // changing the font-drawn radical itself.
    std::vector<std::uint16_t> root_pixels(120 * 80, 0xFFFFU);
    nmarkdown::Surface565 root_surface(root_pixels.data(), 120, 80, 120);
    constexpr int root_origin_x = 8;
    constexpr int root_baseline = 48;
    CHECK(math.draw(root_surface, *constructed_root,
                    root_origin_x, root_baseline, 0,
                    0x29A8U, 0xFFFFU, false, {0, 0, 120, 80}));
    const nmarkdown::MathRule* overbar = nullptr;
    for (const nmarkdown::MathRule& rule : constructed_root->rules) {
        if (rule.width > nmarkdown::fx_from_int(2) &&
            rule.height == nmarkdown::fx_from_int(1)) {
            overbar = &rule;
            break;
        }
    }
    CHECK(overbar != nullptr);
    if (overbar != nullptr) {
        CHECK(overbar->x < radical_glyph_right);
        const int start_x = root_origin_x + nmarkdown::fx_floor(overbar->x);
        const int start_y = root_baseline + nmarkdown::fx_floor(overbar->y);
        CHECK(root_surface.pixel(start_x, start_y) != 0xFFFFU);
        std::vector<std::uint8_t> visited(root_pixels.size(), 0);
        std::vector<int> pending;
        pending.push_back(start_y * 120 + start_x);
        visited[pending.front()] = 1;
        bool reached_check_stroke = false;
        while (!pending.empty()) {
            const int current = pending.back();
            pending.pop_back();
            const int x = current % 120;
            const int y = current / 120;
            if (x < start_x && y >= start_y + 3) reached_check_stroke = true;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if ((dx == 0 && dy == 0) ||
                        x + dx < 0 || x + dx >= 120 ||
                        y + dy < 0 || y + dy >= 80) continue;
                    const int next = (y + dy) * 120 + x + dx;
                    if (visited[next] || root_pixels[next] == 0xFFFFU) continue;
                    visited[next] = 1;
                    pending.push_back(next);
                }
            }
        }
        CHECK(reached_check_stroke);
    }

    std::shared_ptr<const nmarkdown::MathLayoutResult> matrix;
    CHECK(math.layout("\\begin{bmatrix}a&b\\\\c&d\\end{bmatrix}",
                      nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(280), matrix));
    CHECK(matrix->metrics.ascent > nmarkdown::fx_from_int(10));
    CHECK(matrix->runs.size() >= 4);
    CHECK(matrix->rules.empty());
    bool found_latin_modern_square_brackets = false;
    for (const nmarkdown::MathDrawRun& run : matrix->runs) {
        for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
            if (glyph.codepoint == '[' || glyph.codepoint == ']') {
                found_latin_modern_square_brackets = true;
                CHECK(glyph.face == 3);
            }
        }
    }
    CHECK(found_latin_modern_square_brackets);

    std::shared_ptr<const nmarkdown::MathLayoutResult> tall_matrix;
    CHECK(math.layout(
        "\\begin{bmatrix}1&0&0\\\\0&1&0\\\\0&0&1\\end{bmatrix}",
        nmarkdown::MathStyle::Display,
        nmarkdown::fx_from_int(16),
        nmarkdown::fx_from_int(280), tall_matrix));
    bool found_scaled_bracket_glyph = false;
    for (const nmarkdown::MathDrawRun& run : tall_matrix->runs) {
        if (run.glyphs.glyphs.empty() ||
            run.glyphs.glyphs.front().codepoint != '[') {
            continue;
        }
        CHECK(run.glyphs.glyphs.front().face == 3);
        found_scaled_bracket_glyph = true;
    }
    CHECK(found_scaled_bracket_glyph);
    CHECK(tall_matrix->rules.empty());

    std::shared_ptr<const nmarkdown::MathLayoutResult> column_vector;
    CHECK(math.layout("\\begin{pmatrix}x_1\\\\x_2\\end{pmatrix}",
                      nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(280), column_vector));
    CHECK(column_vector != nullptr);
    CHECK(column_vector->rules.empty());
    bool found_latin_modern_parenthesis = false;
    for (const nmarkdown::MathDrawRun& run : column_vector->runs) {
        for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
            if (glyph.codepoint == '(' || glyph.codepoint == ')') {
                found_latin_modern_parenthesis = true;
                CHECK(glyph.face == 3);
                CHECK(glyph.glyph > 1065);
            }
        }
    }
    CHECK(found_latin_modern_parenthesis);

    std::shared_ptr<const nmarkdown::MathLayoutResult> matrix_factor;
    CHECK(math.layout("\\begin{pmatrix}a&b\\\\c&d\\end{pmatrix}",
                      nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(280), matrix_factor));
    std::shared_ptr<const nmarkdown::MathLayoutResult> matrix_product;
    CHECK(math.layout(
        "\\begin{pmatrix}a&b\\\\c&d\\end{pmatrix}"
        "\\begin{pmatrix}x_1\\\\x_2\\end{pmatrix}",
        nmarkdown::MathStyle::Display,
        nmarkdown::fx_from_int(16),
        nmarkdown::fx_from_int(280), matrix_product));
    CHECK(matrix_factor != nullptr && matrix_product != nullptr);
    if (matrix_factor != nullptr && matrix_product != nullptr) {
        CHECK(matrix_product->metrics.width >=
              matrix_factor->metrics.width + column_vector->metrics.width +
                  nmarkdown::fx_from_int(2));
    }

    std::shared_ptr<const nmarkdown::MathLayoutResult> thin_delimiters;
    CHECK(math.layout(
        "\\left\\langle\\frac{a}{b}\\right\\rangle+"
        "\\left\\{\\frac{x}{y}\\right\\}",
        nmarkdown::MathStyle::Display,
        nmarkdown::fx_from_int(16),
        nmarkdown::fx_from_int(280), thin_delimiters));
    CHECK(thin_delimiters != nullptr);
    CHECK(thin_delimiters->rules.size() == 2);
    bool found_latin_modern_angle = false;
    bool found_latin_modern_brace = false;
    for (const nmarkdown::MathDrawRun& run : thin_delimiters->runs) {
        for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
            CHECK(glyph.face == 3);
            found_latin_modern_angle = found_latin_modern_angle ||
                                       glyph.codepoint == 0x27E8U ||
                                       glyph.codepoint == 0x27E9U;
            found_latin_modern_brace = found_latin_modern_brace ||
                                       glyph.codepoint == '{' ||
                                       glyph.codepoint == '}';
        }
    }
    CHECK(found_latin_modern_angle);
    CHECK(found_latin_modern_brace);

    std::shared_ptr<const nmarkdown::MathLayoutResult> braces;
    CHECK(math.layout("\\left\\{\\frac{x}{y}\\right\\}",
                      nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(280), braces));
    CHECK(braces != nullptr);
    if (braces != nullptr) {
        bool found_system_opening = false;
        bool found_system_closing = false;
        for (const nmarkdown::MathDrawRun& run : braces->runs) {
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                if (glyph.face != 3) continue;
                found_system_opening = found_system_opening ||
                                       glyph.codepoint == '{';
                found_system_closing = found_system_closing ||
                                       glyph.codepoint == '}';
                if (glyph.codepoint == '{' || glyph.codepoint == '}') {
                    CHECK(glyph.glyph > 2);
                }
            }
        }
        CHECK(found_system_opening);
        CHECK(found_system_closing);
    }

    std::shared_ptr<const nmarkdown::MathLayoutResult> tall_cases;
    CHECK(math.layout(
        R"(f(x)=\begin{cases}x^2&x<0\\\sqrt{x+1}&0\le x<3\\\sum_{i=0}^{n}x_i^2&x\ge3\end{cases})",
        nmarkdown::MathStyle::Display,
        nmarkdown::fx_from_int(16),
        nmarkdown::fx_from_int(280), tall_cases));
    bool found_tall_system_brace = false;
    if (tall_cases != nullptr) {
        for (const nmarkdown::MathDrawRun& run : tall_cases->runs) {
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                if (glyph.face == 3 && glyph.codepoint == '{') {
                    found_tall_system_brace = true;
                    CHECK(run.pixel_size > nmarkdown::fx_from_int(16));
                }
            }
        }
    }
    CHECK(found_tall_system_brace);

    std::shared_ptr<const nmarkdown::MathLayoutResult> variants;
    CHECK(math.layout("x+\\mathrm{x}+\\mathit{x}+\\mathbf{x}",
                      nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(280), variants));
    unsigned mathematical_italic_x = 0;
    unsigned roman_x = 0;
    unsigned mathematical_bold_x = 0;
    for (const nmarkdown::MathDrawRun& run : variants->runs) {
        for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
            CHECK(glyph.face == 3);
            if (glyph.codepoint == 0x1D465U) ++mathematical_italic_x;
            if (glyph.codepoint == 'x') ++roman_x;
            if (glyph.codepoint == 0x1D431U) ++mathematical_bold_x;
        }
    }
    CHECK(mathematical_italic_x == 2);
    CHECK(roman_x == 1);
    CHECK(mathematical_bold_x == 1);

    std::shared_ptr<const nmarkdown::MathLayoutResult> bold_symbols;
    CHECK(math.layout(R"(\bm{x}+\boldsymbol\Gamma+\bm{\vartheta}+\bm{7}+\mathbf{x})",
                      nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(280), bold_symbols));
    const std::array<std::uint32_t, 5> expected_bold_symbols{{
        0x1D499U,  // mathematical bold italic small x
        0x1D71EU,  // mathematical bold italic capital Gamma
        0x1D751U,  // mathematical bold italic theta symbol
        0x1D7D5U,  // mathematical bold digit seven (digits stay upright)
        0x1D431U,  // mathbf remains upright bold
    }};
    std::size_t bold_symbol_index = 0;
    if (bold_symbols != nullptr) {
        for (const nmarkdown::MathDrawRun& run : bold_symbols->runs) {
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                if (glyph.codepoint == '+') continue;
                CHECK(bold_symbol_index < expected_bold_symbols.size());
                if (bold_symbol_index < expected_bold_symbols.size()) {
                    CHECK(glyph.codepoint ==
                          expected_bold_symbols[bold_symbol_index]);
                }
                ++bold_symbol_index;
            }
        }
    }
    CHECK(bold_symbol_index == expected_bold_symbols.size());

    const auto rendered_ink = [&](const char* formula,
                                  nmarkdown::MathStyle draw_style) {
        std::shared_ptr<const nmarkdown::MathLayoutResult> result;
        CHECK(math.layout(formula, draw_style, nmarkdown::fx_from_int(16),
                          nmarkdown::fx_from_int(120), result));
        CHECK(result != nullptr && result->valid);
        std::vector<std::uint16_t> pixels(120 * 80, 0xFFFFU);
        nmarkdown::Surface565 surface(pixels.data(), 120, 80, 120);
        if (result != nullptr) {
            CHECK(math.draw(surface, *result, 8, 48, 0, 0x2948U, 0xFFFFU,
                            false, {0, 0, 120, 80}));
        }
        return static_cast<std::size_t>(std::count_if(
            pixels.begin(), pixels.end(),
            [](std::uint16_t pixel) { return pixel != 0xFFFFU; }));
    };
    CHECK(rendered_ink(R"(\bm{+})", nmarkdown::MathStyle::Text) >
          rendered_ink("+", nmarkdown::MathStyle::Text));
    CHECK(rendered_ink(R"(\boldsymbol{\sum})", nmarkdown::MathStyle::Display) >
          rendered_ink(R"(\sum)", nmarkdown::MathStyle::Display));

    std::shared_ptr<const nmarkdown::MathLayoutResult> styled_greek;
    CHECK(math.layout(
        R"(\mathit{\Gamma\Sigma\Omega}+\mathbf{\Gamma\Sigma\Phi\Omega})",
        nmarkdown::MathStyle::Display, nmarkdown::fx_from_int(16),
        nmarkdown::fx_from_int(280), styled_greek));
    const std::array<std::uint32_t, 7> expected_styled_greek{{
        0x1D6E4U, 0x1D6F4U, 0x1D6FAU,
        0x1D6AAU, 0x1D6BAU, 0x1D6BDU, 0x1D6C0U,
    }};
    std::size_t styled_greek_index = 0;
    if (styled_greek != nullptr) {
        for (const nmarkdown::MathDrawRun& run : styled_greek->runs) {
            CHECK(run.glyphs.substitution_count == 0);
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                if (glyph.codepoint == '+') continue;
                CHECK(styled_greek_index < expected_styled_greek.size());
                if (styled_greek_index < expected_styled_greek.size()) {
                    CHECK(glyph.codepoint ==
                          expected_styled_greek[styled_greek_index]);
                }
                ++styled_greek_index;
            }
        }
    }
    CHECK(styled_greek_index == expected_styled_greek.size());

    std::shared_ptr<const nmarkdown::MathLayoutResult> bold_italic_letters;
    CHECK(math.layout("\\imath+\\jmath+\\hat{\\imath}",
                      nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(280), bold_italic_letters));
    unsigned bold_italic_i = 0;
    unsigned bold_italic_j = 0;
    if (bold_italic_letters != nullptr) {
        for (const nmarkdown::MathDrawRun& run : bold_italic_letters->runs) {
            CHECK(run.glyphs.substitution_count == 0);
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                CHECK(glyph.face == 3);
                if (glyph.codepoint == 0x1D48AU) {
                    ++bold_italic_i;
                    CHECK(glyph.glyph == 1658);  // Native dotted bold-italic i.
                }
                if (glyph.codepoint == 0x1D48BU) {
                    ++bold_italic_j;
                    CHECK(glyph.glyph == 1659);  // Native dotted bold-italic j.
                }
            }
        }
    }
    CHECK(bold_italic_i == 2);
    CHECK(bold_italic_j == 1);

    std::shared_ptr<const nmarkdown::MathLayoutResult> special_alphabets;
    CHECK(math.layout("\\mathbb{R}+\\mathcal{F}",
                      nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(280), special_alphabets));
    bool found_double_struck_r = false;
    bool found_script_f = false;
    for (const nmarkdown::MathDrawRun& run : special_alphabets->runs) {
        if (run.glyphs.glyphs.empty()) continue;
        found_double_struck_r = found_double_struck_r ||
            run.glyphs.glyphs.front().codepoint == 0x211DU;
        found_script_f = found_script_f ||
            run.glyphs.glyphs.front().codepoint == 0x2131U;
    }
    CHECK(found_double_struck_r);
    CHECK(found_script_f);

    std::shared_ptr<const nmarkdown::MathLayoutResult> all_latin_modern;
    CHECK(math.layout(
        R"(\frac{-b\pm\sqrt{b^2-4ac}}{2a}+\sum_{i=0}^{n}\alpha_i+\left\langle\mathbb{R},\mathcal{F}\right\rangle)",
        nmarkdown::MathStyle::Display,
        nmarkdown::fx_from_int(16),
        nmarkdown::fx_from_int(280), all_latin_modern));
    std::size_t latin_modern_glyph_count = 0;
    for (const nmarkdown::MathDrawRun& run : all_latin_modern->runs) {
        for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
            ++latin_modern_glyph_count;
            CHECK(glyph.face == 3);
        }
    }
    CHECK(latin_modern_glyph_count > 20);

    nmarkdown::GlyphRun unsupported_math;
    constexpr char unsupported_sample[] = u8"中";
    CHECK(text.shape(unsupported_sample, sizeof(unsupported_sample) - 1,
                     nmarkdown::fx_from_int(16), unsupported_math,
                     nmarkdown::FontRole::Math));
    CHECK(unsupported_math.substitution_count == 1);
    CHECK(unsupported_math.glyphs.size() == 1);
    if (!unsupported_math.glyphs.empty()) {
        CHECK(unsupported_math.glyphs.front().face == 3);
    }

    std::shared_ptr<const nmarkdown::MathLayoutResult> accent;
    CHECK(math.layout("\\hat{x}", nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(280), accent));
    bool found_base = false;
    bool found_mark = false;
    for (const nmarkdown::MathDrawRun& run : accent->runs) {
        CHECK(run.glyphs.substitution_count == 0);
        for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
            CHECK(glyph.face == 3);
            found_base = found_base || glyph.codepoint == 0x1D465U;
            if (glyph.codepoint == 0x0302U) {
                found_mark = true;
                CHECK(glyph.combining_mark);
                CHECK(glyph.x_advance == 0);
            }
        }
    }
    CHECK(found_base && found_mark);

    std::vector<std::uint16_t> pixels(320 * 120, 0xFFFFU);
    nmarkdown::Surface565 surface(pixels.data(), 320, 120, 320);
    CHECK(math.draw(surface, *fraction, 10, 60, 0, 0, 0xFFFFU,
                    false, {0, 0, 320, 120}));
    std::size_t changed = 0;
    for (std::uint16_t pixel : pixels) {
        if (pixel != 0xFFFFU) ++changed;
    }
    CHECK(changed > 20);
}

void test_dark_rule_coverage_matches_glyph_correction() {
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    nmarkdown::MathLayoutResult layout;
    layout.rules.push_back({0, 0, nmarkdown::fx_from_int(1),
                            nmarkdown::fx_from_int(1), 128});

    const std::uint16_t black = nmarkdown::rgb565(0, 0, 0);
    const std::uint16_t white = nmarkdown::rgb565(255, 255, 255);
    std::vector<std::uint16_t> pixels(2, black);
    nmarkdown::Surface565 surface(pixels.data(), 2, 1, 2);
    CHECK(math.draw(surface, layout, 0, 0, 0, white, black, true,
                    surface.bounds()));
    CHECK(surface.pixel(0, 0) == nmarkdown::blend565(
              black, white, text.coverage_lut(true)[128]));

    surface.clear(black);
    CHECK(math.draw(surface, layout, 0, 0, 0, white, black, false,
                    surface.bounds()));
    CHECK(surface.pixel(0, 0) == nmarkdown::blend565(
              black, white, text.coverage_lut(false)[128]));
    CHECK(text.coverage_lut(false)[64] <
          nmarkdown::make_coverage_lut(36)[64]);
    CHECK(text.coverage_lut(true)[128] > 128);
}

void test_complete_native_symbol_layout() {
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);

    for (const ExpectedSymbol& expected : kExpectedSymbols) {
        const std::string formula = "\\" + std::string(expected.name);
        std::shared_ptr<const nmarkdown::MathLayoutResult> result;
        const bool laid_out = math.layout(
            formula, nmarkdown::MathStyle::Display,
            nmarkdown::fx_from_int(16), nmarkdown::fx_from_int(310), result);
        if (!laid_out || result == nullptr || !result->valid) {
            std::fprintf(stderr, "native symbol layout \\%s failed: %s\n",
                         expected.name,
                         result == nullptr ? "no result" :
                                             result->diagnostic.c_str());
            ++failures;
            continue;
        }
        CHECK(!result->runs.empty());
        for (const nmarkdown::MathDrawRun& run : result->runs) {
            CHECK(run.glyphs.substitution_count == 0);
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                CHECK(glyph.face == 3);
                CHECK(glyph.glyph != 0);
            }
        }
    }

    const std::array<std::pair<const char*, nmarkdown::GlyphId>, 8> anchors{{
        {"\\cup", 2764}, {"\\rightarrow", 1858}, {"\\N", 3506},
        {"\\lang", 2579}, {"\\rang", 2580}, {"\\empty", 2786},
        {"\\or", 2770}, {"\\varepsilon", 4483},
    }};
    for (const auto& anchor : anchors) {
        std::shared_ptr<const nmarkdown::MathLayoutResult> result;
        CHECK(math.layout(anchor.first, nmarkdown::MathStyle::Text,
                          nmarkdown::fx_from_int(16), nmarkdown::fx_from_int(310),
                          result));
        bool found = false;
        if (result != nullptr) {
            for (const nmarkdown::MathDrawRun& run : result->runs) {
                for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                    found = found || glyph.glyph == anchor.second;
                }
            }
        }
        if (!found) {
            std::fprintf(stderr, "native symbol anchor %s did not use gid %u\n",
                         anchor.first, static_cast<unsigned>(anchor.second));
            ++failures;
        }
    }

    const std::array<const char*, 2> visible_space_cases{{
        R"(\textvisiblespace)", R"(\text{A\textvisiblespace B})",
    }};
    for (const char* formula : visible_space_cases) {
        std::shared_ptr<const nmarkdown::MathLayoutResult> result;
        CHECK(math.layout(formula, nmarkdown::MathStyle::Text,
                          nmarkdown::fx_from_int(16),
                          nmarkdown::fx_from_int(310), result));
        CHECK(result != nullptr && result->valid);
        bool found = false;
        if (result != nullptr) {
            for (const nmarkdown::MathDrawRun& run : result->runs) {
                CHECK(run.glyphs.substitution_count == 0);
                for (const nmarkdown::PositionedGlyph& glyph :
                     run.glyphs.glyphs) {
                    if (glyph.codepoint != 0x2423U) continue;
                    found = true;
                    CHECK(glyph.face == 3);
                    CHECK(glyph.glyph == 228);
                    CHECK(glyph.x_advance > 0);
                }
            }
        }
        CHECK(found);
    }

    const std::array<const char*, 9> accents{{
        "\\dot x", "\\ddot{x}", "\\acute x", "\\grave{x}",
        "\\breve x", "\\check{x}", "\\tilde x", "\\mathring{x}",
        "\\vec{x}",
    }};
    for (const char* formula : accents) {
        std::shared_ptr<const nmarkdown::MathLayoutResult> result;
        CHECK(math.layout(formula, nmarkdown::MathStyle::Text,
                          nmarkdown::fx_from_int(16), nmarkdown::fx_from_int(310),
                          result));
        CHECK(result != nullptr && result->valid);
        if (result == nullptr) continue;
        bool found_combining = false;
        for (const nmarkdown::MathDrawRun& run : result->runs) {
            CHECK(run.glyphs.substitution_count == 0);
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                found_combining = found_combining || glyph.combining_mark;
            }
        }
        CHECK(found_combining);
    }

    std::shared_ptr<const nmarkdown::MathLayoutResult> negated;
    CHECK(math.layout(R"(\not=+\not\in+\not\rightarrow)",
                      nmarkdown::MathStyle::Text, nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(310), negated));
    CHECK(negated != nullptr && negated->valid);
    if (negated != nullptr) {
        for (const nmarkdown::MathDrawRun& run : negated->runs) {
            CHECK(run.glyphs.substitution_count == 0);
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                CHECK(glyph.face == 3);
                CHECK(glyph.glyph != 0);
            }
        }
    }

    std::shared_ptr<const nmarkdown::MathLayoutResult> lowercase_calligraphic;
    CHECK(math.layout(R"(\mathcal{abcdefghijklmnopqrstuvwxyz})",
                      nmarkdown::MathStyle::Text, nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(310), lowercase_calligraphic));
    CHECK(lowercase_calligraphic != nullptr && lowercase_calligraphic->valid);
    if (lowercase_calligraphic != nullptr) {
        for (const nmarkdown::MathDrawRun& run : lowercase_calligraphic->runs) {
            CHECK(run.glyphs.substitution_count == 0);
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                CHECK(glyph.face == 3);
                CHECK(glyph.glyph != 0);
            }
        }
    }
}

void test_local_error_box() {
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    std::shared_ptr<const nmarkdown::MathLayoutResult> result;
    CHECK(math.layout("\\frac{x}{", nmarkdown::MathStyle::Text,
                      nmarkdown::fx_from_int(15), nmarkdown::fx_from_int(200), result));
    CHECK(result != nullptr);
    CHECK(!result->valid);
    CHECK(!result->diagnostic.empty());
    CHECK(!result->runs.empty());

    std::string wide;
    for (int index = 0; index < 80; ++index) wide += "x+";
    CHECK(math.layout(wide, nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(15), nmarkdown::fx_from_int(60), result));
    CHECK(result->overflow);

    for (int index = 0; index < 70; ++index) {
        const std::string formula = "x_" + std::to_string(index);
        CHECK(math.layout(formula, nmarkdown::MathStyle::Text,
                          nmarkdown::fx_from_int(15), nmarkdown::fx_from_int(200), result));
    }
    CHECK(math.cache_stats().entries <= 64);
    CHECK(math.cache_stats().evictions > 0);
}

void test_markdown_formula_document_layout() {
    const std::array<const char*, 12> formulas{{
        R"(\alpha+\zeta+\eta+\iota+\kappa+\nu+\xi+\omicron+\tau+\upsilon+\varphi+\chi)",
        R"(\ngeq+\ast+\because+\therefore+\backsim+\cong+\forall+\exists+\angle)",
        R"(\iint+\iiint+\oint+\bigodot+\bigoplus+\bigotimes)",
        R"(\overleftarrow{a+b+c}+\overrightarrow{a+b+c}+\underleftrightarrow{a+b+c})",
        R"(\overbrace{a+\underbrace{b+c}_{1.0}}^{2.0})",
        R"(\rm D+\cal D+\it D+\Bbb D+\bf D+\sf D+\tt D+\frak D)",
        R"(\begin{Bmatrix}1&2\\3&4\end{Bmatrix}\begin{vmatrix}a&b\\c&d\end{vmatrix}\begin{Vmatrix}x&y\\z&w\end{Vmatrix})",
        R"(\begin{array}{c|lcr}n&\text{左对齐}&\text{居中对齐}&\text{右对齐}\\\hline1&0.24&1&125\end{array})",
        R"(f(n)=\begin{cases}n/2&\text{if $n$ is even}\\3n+1&\text{if $n$ is odd}\end{cases})",
        R"(\begin{align}v+w&=0&\text{Given}\tag 1\\-w&=-w+0&\text{identity}\tag 2\end{align})",
        R"(x=a_0+\cfrac{1}{a_1+\cfrac{2^2}{a_2+\cdots}})",
        R"(f\left(\left[\frac{1+\left\{x,y\right\}}{\left(\frac{x}{y}+\frac{y}{x}\right)}\right]^{3/2}\right)\tag{公式1})",
    }};

    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    for (std::size_t index = 0; index < formulas.size(); ++index) {
        std::shared_ptr<const nmarkdown::MathLayoutResult> result;
        const bool laid_out = math.layout(
            formulas[index], nmarkdown::MathStyle::Display,
            nmarkdown::fx_from_int(16), nmarkdown::fx_from_int(310), result);
        if (!laid_out || result == nullptr || !result->valid) {
            std::fprintf(stderr, "formula layout case %zu: %s\n", index + 1,
                         result == nullptr ? "no result" : result->diagnostic.c_str());
            ++failures;
            continue;
        }
        CHECK(result->metrics.width > 0);
        CHECK(!result->runs.empty() || !result->rules.empty());
        std::size_t substitutions = 0;
        for (const nmarkdown::MathDrawRun& run : result->runs) {
            substitutions += run.glyphs.substitution_count;
        }
        // Cases 8 and 12 deliberately exercise CJK text inside math. Whether
        // those glyphs are present depends on the separately selected CJK family;
        // the math engine's responsibility is to preserve and route the run.
        const bool external_cjk_text = index == 7 || index == 11;
        if (substitutions != 0 && !external_cjk_text) {
            std::fprintf(stderr,
                         "formula layout case %zu used %zu replacement glyph(s)\n",
                         index + 1, substitutions);
            ++failures;
        }
    }

    std::shared_ptr<const nmarkdown::MathLayoutResult> ruled_array;
    CHECK(math.layout(R"(\begin{array}{c|l}a&b\\\hline c&d\end{array})",
                      nmarkdown::MathStyle::Display, nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(310), ruled_array));
    CHECK(ruled_array != nullptr);
    if (ruled_array != nullptr) CHECK(ruled_array->rules.size() >= 2);

    std::shared_ptr<const nmarkdown::MathLayoutResult> promoted_style;
    CHECK(math.layout(R"(\int+\displaystyle\int)", nmarkdown::MathStyle::Text,
                      nmarkdown::fx_from_int(16), nmarkdown::fx_from_int(310),
                      promoted_style));
    std::vector<nmarkdown::GlyphId> integral_glyphs;
    if (promoted_style != nullptr) {
        for (const nmarkdown::MathDrawRun& run : promoted_style->runs) {
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                if (glyph.codepoint == 0x222BU) {
                    integral_glyphs.push_back(glyph.glyph);
                }
            }
        }
    }
    CHECK(integral_glyphs.size() == 2);
    if (integral_glyphs.size() == 2) {
        CHECK(integral_glyphs[0] == 3049);
        CHECK(integral_glyphs[1] == 3063);
    }
}

void test_large_align_formula_layout_and_math_text_face() {
    constexpr const char* source = R"(\begin{align}
    v + w & = 0  & \text{Given} \tag 1 \\
       -w & = -w + 0 & \text{additive identity} \tag 2 \\
   -w + 0 & = -w + (v + w) & \text{equations $(1)$ and $(2)$} \\
\end{align})";
    // 320 px screen minus the Viewer's default 5 px side margins.
    constexpr int kCalculatorContentWidth = 310;

    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    std::shared_ptr<const nmarkdown::MathLayoutResult> result;
    CHECK(math.layout(source, nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(kCalculatorContentWidth), result));
    CHECK(result != nullptr);
    if (result == nullptr) return;
    CHECK(result->valid);
    CHECK(result->diagnostic.empty());
    CHECK(result->overflow);
    CHECK(result->metrics.width > nmarkdown::fx_from_int(kCalculatorContentWidth));

    // All three align rows must retain separate baselines.  This catches a
    // regression where a long explanatory cell overlaps another row.
    std::vector<nmarkdown::Fx> baselines;
    for (const nmarkdown::MathDrawRun& run : result->runs) {
        baselines.push_back(run.baseline_y);
        CHECK(run.x >= 0);
        CHECK(run.x + run.glyphs.width <= result->metrics.width);
    }
    std::sort(baselines.begin(), baselines.end());
    baselines.erase(std::unique(baselines.begin(), baselines.end()),
                    baselines.end());
    CHECK(baselines.size() >= 3);

    // `align` alternates right- and left-aligned cells around each `&`.
    // Exercise the exact oversized review formula, rather than merely checking
    // that all of its runs remain inside the computed box: the three relation
    // signs must begin at one alignment point and all three annotation cells
    // must share one right edge. Equation tags occupy a separate lane and must
    // never pull an untagged annotation out of its alignment column.
    std::vector<nmarkdown::Fx> relation_x;
    std::vector<nmarkdown::Fx> annotation_right;
    for (const nmarkdown::MathDrawRun& run : result->runs) {
        if (run.glyphs.glyphs.size() == 1 &&
            run.glyphs.glyphs.front().codepoint ==
                static_cast<std::uint32_t>('=')) {
            relation_x.push_back(run.x);
        }

        if (run.glyphs.glyphs.size() < 5) continue;
        const std::uint32_t first = run.glyphs.glyphs.front().codepoint;
        if (first == static_cast<std::uint32_t>('G') ||
            first == static_cast<std::uint32_t>('a') ||
            first == static_cast<std::uint32_t>('e')) {
            annotation_right.push_back(run.x + run.glyphs.width);
        }
    }
    CHECK(relation_x.size() == 3);
    if (relation_x.size() == 3) {
        CHECK(relation_x[0] == relation_x[1]);
        CHECK(relation_x[1] == relation_x[2]);
    }
    CHECK(annotation_right.size() == 3);
    if (annotation_right.size() == 3) {
        CHECK(annotation_right[0] == annotation_right[1]);
        CHECK(annotation_right[1] == annotation_right[2]);
        CHECK(annotation_right[2] < result->metrics.width);
    }

    // A literal G occurs only in \text{Given}.  Compare its selected face to
    // explicit Math and Body shaping so the test observes the requested
    // LaTeX-face routing without relying on private font collection state.
    nmarkdown::GlyphRun math_g;
    nmarkdown::GlyphRun body_g;
    CHECK(text.shape("G", 1, nmarkdown::fx_from_int(16), math_g,
                     nmarkdown::FontRole::Math));
    CHECK(text.shape("G", 1, nmarkdown::fx_from_int(16), body_g,
                     nmarkdown::FontRole::BodySans));
    CHECK(!math_g.glyphs.empty());
    CHECK(!body_g.glyphs.empty());
    if (math_g.glyphs.empty() || body_g.glyphs.empty()) return;
    const nmarkdown::FontFaceId math_face = math_g.glyphs.front().face;
    CHECK(math_face != body_g.glyphs.front().face);

    bool found_given_g = false;
    for (const nmarkdown::MathDrawRun& run : result->runs) {
        for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
            if (glyph.codepoint != static_cast<std::uint32_t>('G')) continue;
            found_given_g = true;
            CHECK(glyph.face == math_face);
        }
    }
    CHECK(found_given_g);

    // Mixed-script annotations still keep supported Latin clusters in Latin
    // Modern Math; only unsupported CJK codepoints use the fallback chain.
    // Falling back the whole string would silently return "Given" to BodySans.
    std::shared_ptr<const nmarkdown::MathLayoutResult> mixed_annotation;
    CHECK(math.layout(R"(\text{Given公式})", nmarkdown::MathStyle::Text,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(kCalculatorContentWidth),
                      mixed_annotation));
    CHECK(mixed_annotation != nullptr);
    bool found_mixed_g = false;
    if (mixed_annotation != nullptr) {
        for (const nmarkdown::MathDrawRun& run : mixed_annotation->runs) {
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                if (glyph.codepoint != static_cast<std::uint32_t>('G')) continue;
                found_mixed_g = true;
                CHECK(glyph.face == math_face);
            }
        }
    }
    CHECK(found_mixed_g);

    // Different-width tags make the independent right-aligned tag lane
    // observable. Both labels must terminate at the oversized canvas's exact
    // right edge; viewport panning may translate this coordinate, but must not
    // change the alignment within the formula.
    std::shared_ptr<const nmarkdown::MathLayoutResult> unequal_tags;
    CHECK(math.layout(
        R"(\begin{align}a&=b&\text{short}\tag{1}\\c&=d&\text{longer}\tag{100}\end{align})",
        nmarkdown::MathStyle::Display, nmarkdown::fx_from_int(16),
        nmarkdown::fx_from_int(80), unequal_tags));
    CHECK(unequal_tags != nullptr);
    if (unequal_tags != nullptr) {
        CHECK(unequal_tags->overflow);
        std::vector<nmarkdown::Fx> tag_right;
        for (const nmarkdown::MathDrawRun& run : unequal_tags->runs) {
            if (run.glyphs.glyphs.size() < 3 ||
                run.glyphs.glyphs.front().codepoint !=
                    static_cast<std::uint32_t>('(') ||
                run.glyphs.glyphs.back().codepoint !=
                    static_cast<std::uint32_t>(')')) {
                continue;
            }
            tag_right.push_back(run.x + run.glyphs.width);
        }
        CHECK(tag_right.size() == 2);
        for (nmarkdown::Fx right : tag_right) {
            CHECK(right == unequal_tags->metrics.width);
        }
    }

    // A tag is a row label, not another alignment-cell atom. It must leave all
    // equation-column coordinates unchanged and occupy a separate right lane.
    constexpr const char* tagged_source =
        R"(\begin{align}a&=b&\text{left}\tag 1\\c&=d&\text{right}\end{align})";
    constexpr const char* untagged_source =
        R"(\begin{align}a&=b&\text{left}\\c&=d&\text{right}\end{align})";
    std::shared_ptr<const nmarkdown::MathLayoutResult> tagged;
    std::shared_ptr<const nmarkdown::MathLayoutResult> untagged;
    CHECK(math.layout(tagged_source, nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(kCalculatorContentWidth), tagged));
    CHECK(math.layout(untagged_source, nmarkdown::MathStyle::Display,
                      nmarkdown::fx_from_int(16),
                      nmarkdown::fx_from_int(kCalculatorContentWidth), untagged));
    CHECK(tagged != nullptr && untagged != nullptr);
    if (tagged != nullptr && untagged != nullptr) {
        std::vector<nmarkdown::Fx> tagged_equation_x;
        std::vector<nmarkdown::Fx> untagged_equation_x;
        const nmarkdown::MathDrawRun* tag_run = nullptr;
        for (const nmarkdown::MathDrawRun& run : tagged->runs) {
            bool contains_tag_parenthesis = false;
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                contains_tag_parenthesis = contains_tag_parenthesis ||
                    glyph.codepoint == static_cast<std::uint32_t>('(') ||
                    glyph.codepoint == static_cast<std::uint32_t>(')');
            }
            if (contains_tag_parenthesis) {
                tag_run = &run;
            } else {
                tagged_equation_x.push_back(run.x);
            }
        }
        for (const nmarkdown::MathDrawRun& run : untagged->runs) {
            untagged_equation_x.push_back(run.x);
        }
        CHECK(tag_run != nullptr);
        CHECK(tagged_equation_x.size() == untagged_equation_x.size());
        if (tagged_equation_x.size() == untagged_equation_x.size()) {
            CHECK(!tagged_equation_x.empty());
            for (std::size_t index = 0;
                 index < tagged_equation_x.size(); ++index) {
                CHECK(tagged_equation_x[index] == untagged_equation_x[index]);
            }
        }
        if (tag_run != nullptr) {
            CHECK(tag_run->x + tag_run->glyphs.width == tagged->metrics.width);
            if (!tagged->runs.empty() && !untagged->runs.empty()) {
                const nmarkdown::MathDrawRun& final_content = tagged->runs.back();
                const nmarkdown::MathDrawRun& untagged_final =
                    untagged->runs.back();
                CHECK(final_content.x + final_content.glyphs.width ==
                      untagged_final.x + untagged_final.glyphs.width);
                CHECK(final_content.x + final_content.glyphs.width <
                      tagged->metrics.width);
            }
            for (const nmarkdown::PositionedGlyph& glyph : tag_run->glyphs.glyphs) {
                CHECK(glyph.face == math_face);
            }
        }
    }

    // `aligned` shares the same tag-lane behavior as `align` even though its
    // result fits inside another expression rather than spanning a display.
    std::shared_ptr<const nmarkdown::MathLayoutResult> aligned_tag;
    CHECK(math.layout(
        R"(\begin{aligned}a&=b\tag{A}\\c&=d\end{aligned})",
        nmarkdown::MathStyle::Display, nmarkdown::fx_from_int(16),
        nmarkdown::fx_from_int(kCalculatorContentWidth), aligned_tag));
    CHECK(aligned_tag != nullptr);
    bool found_aligned_tag = false;
    if (aligned_tag != nullptr) {
        for (const nmarkdown::MathDrawRun& run : aligned_tag->runs) {
            for (const nmarkdown::PositionedGlyph& glyph : run.glyphs.glyphs) {
                if (glyph.codepoint == static_cast<std::uint32_t>('(')) {
                    found_aligned_tag = true;
                }
            }
        }
    }
    CHECK(found_aligned_tag);
}

}  // namespace

int main() {
    test_layout_and_cache();
    test_dark_rule_coverage_matches_glyph_correction();
    test_complete_native_symbol_layout();
    test_local_error_box();
    test_markdown_formula_document_layout();
    test_large_align_formula_layout_and_math_text_face();
    if (failures != 0) {
        std::fprintf(stderr, "%d math layout test(s) failed\n", failures);
        return 1;
    }
    std::printf("All math layout tests passed\n");
    return 0;
}
