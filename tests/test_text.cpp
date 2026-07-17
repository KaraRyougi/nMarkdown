#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "nmarkdown/generated/core_font_pack.h"
#include "nmarkdown/io/block_cached_random_access.h"
#include "nmarkdown/platform/platform.h"
#include "nmarkdown/render/surface565.h"
#include "nmarkdown/text/compositor.h"
#include "nmarkdown/text/font.h"
#include "nmarkdown/text/font_pack.h"
#include "nmarkdown/text/glyph_cache.h"
#include "nmarkdown/text/harfbuzz_shaper.h"
#include "nmarkdown/text/text_renderer.h"
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

class CountingRandomAccess final : public nmarkdown::RandomAccessData {
public:
    explicit CountingRandomAccess(std::vector<std::uint8_t> bytes)
        : bytes_(std::move(bytes)) {}

    std::uint64_t size() const override { return bytes_.size(); }

    bool read(std::uint64_t offset,
              std::uint8_t* data,
              std::size_t size) override {
        if (offset > bytes_.size() ||
            size > bytes_.size() - offset) {
            return false;
        }
        if (size != 0) {
            std::memcpy(data, bytes_.data() + offset, size);
        }
        ++read_calls;
        bytes_read += size;
        return true;
    }

    std::size_t read_calls = 0;
    std::size_t bytes_read = 0;

private:
    std::vector<std::uint8_t> bytes_;
};

class PartiallyFailingRandomAccess final
    : public nmarkdown::RandomAccessData {
public:
    explicit PartiallyFailingRandomAccess(
        std::vector<std::uint8_t> bytes)
        : bytes_(std::move(bytes)) {}

    std::uint64_t size() const override { return bytes_.size(); }

    bool read(std::uint64_t offset,
              std::uint8_t* data,
              std::size_t size) override {
        if (offset > bytes_.size() ||
            size > bytes_.size() - offset) {
            return false;
        }
        ++read_calls;
        if (fail_next) {
            const std::size_t partial = size / 2;
            if (partial != 0) {
                std::memcpy(
                    data, bytes_.data() + offset, partial);
            }
            fail_next = false;
            return false;
        }
        if (size != 0) {
            std::memcpy(data, bytes_.data() + offset, size);
        }
        return true;
    }

    bool fail_next = false;
    std::size_t read_calls = 0;

private:
    std::vector<std::uint8_t> bytes_;
};

std::uint16_t read_be16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) << 8U | bytes[1];
}

std::uint32_t read_be32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) << 24U |
           static_cast<std::uint32_t>(bytes[1]) << 16U |
           static_cast<std::uint32_t>(bytes[2]) << 8U | bytes[3];
}

void write_be16(std::uint8_t* bytes, std::uint16_t value) {
    bytes[0] = static_cast<std::uint8_t>(value >> 8U);
    bytes[1] = static_cast<std::uint8_t>(value);
}

bool mark_font_bold(std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < 12U) return false;
    const std::size_t count = read_be16(bytes.data() + 4);
    if (count > (bytes.size() - 12U) / 16U) return false;
    bool changed_os2 = false;
    bool changed_head = false;
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint8_t* record = bytes.data() + 12U + index * 16U;
        const std::uint32_t offset = read_be32(record + 8);
        const std::uint32_t length = read_be32(record + 12);
        if (offset > bytes.size() || length > bytes.size() - offset) continue;
        if (record[0] == 'O' && record[1] == 'S' && record[2] == '/' &&
            record[3] == '2' && length >= 64U) {
            write_be16(bytes.data() + offset + 4, 700);
            write_be16(bytes.data() + offset + 62,
                       read_be16(bytes.data() + offset + 62) | 0x0020U);
            changed_os2 = true;
        }
        if (record[0] == 'h' && record[1] == 'e' && record[2] == 'a' &&
            record[3] == 'd' && length >= 46U) {
            write_be16(bytes.data() + offset + 44,
                       read_be16(bytes.data() + offset + 44) | 0x0001U);
            changed_head = true;
        }
    }
    return changed_os2 && changed_head;
}

struct TextFixture {
    nmarkdown::FontPack pack;
    nmarkdown::FontCollection fonts;
    std::vector<std::uint8_t> body;
    std::vector<std::uint8_t> italic;
    std::vector<std::uint8_t> monospace;
    std::string error;

    bool initialize() {
        if (!read_font_asset("DejaVuSans-CX.ttf", body) ||
            !read_font_asset("DejaVuSans-Oblique-CX.ttf", italic) ||
            !read_font_asset("DejaVuSansMono-CX.ttf", monospace)) {
            error = "could not read external DejaVu test faces";
            return false;
        }
        if (!pack.load_from_memory(nmarkdown::kCoreFontPack,
                                   nmarkdown::kCoreFontPackSize, error)) {
            return false;
        }
        const std::vector<nmarkdown::MemoryFontFace> external = {
            {2001, nmarkdown::FontRole::BodySans, body.data(), body.size(),
             nullptr},
            {2002, nmarkdown::FontRole::BodySansItalic,
             italic.data(), italic.size(), nullptr},
            {2003, nmarkdown::FontRole::Monospace,
             monospace.data(), monospace.size(), nullptr},
        };
        return fonts.initialize(pack, external, error);
    }
};

nmarkdown::GlyphRun shape(nmarkdown::HarfBuzzShaper& shaper,
                          const char* text,
                          int pixel_size = 16,
                          nmarkdown::FontRole preferred_role =
                              nmarkdown::FontRole::BodySans,
                          nmarkdown::TextSpacing spacing =
                              nmarkdown::TextSpacing::Natural) {
    nmarkdown::GlyphRun run;
    CHECK(shaper.shape(reinterpret_cast<const std::uint8_t*>(text),
                       std::char_traits<char>::length(text),
                       nmarkdown::fx_from_int(pixel_size),
                       run,
                       preferred_role,
                       spacing));
    return run;
}

void test_role_aware_shaping() {
    TextFixture fixture;
    CHECK(fixture.initialize());
    nmarkdown::HarfBuzzShaper shaper(fixture.fonts);

    const nmarkdown::GlyphRun body = shape(shaper, "code");
    const nmarkdown::GlyphRun code = shape(
        shaper, "code", 16, nmarkdown::FontRole::Monospace);
    CHECK(body.glyphs.size() == code.glyphs.size());
    CHECK(std::all_of(body.glyphs.begin(), body.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2001;
                      }));
    CHECK(std::all_of(code.glyphs.begin(), code.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2003;
                      }));
    CHECK(!code.glyphs.empty());
    if (!code.glyphs.empty()) {
        const nmarkdown::Fx advance = code.glyphs.front().x_advance;
        CHECK(std::all_of(code.glyphs.begin(), code.glyphs.end(),
                          [advance](const nmarkdown::PositionedGlyph& glyph) {
                              return glyph.x_advance == advance;
                          }));
    }
}

void test_native_and_outline_fallback_italics() {
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    std::vector<std::uint8_t> body;
    std::vector<std::uint8_t> italic;
    CHECK(read_font_asset("DejaVuSans-CX.ttf", body));
    CHECK(read_font_asset("DejaVuSans-Oblique-CX.ttf", italic));
    CHECK(text.set_external_font(nmarkdown::FontRole::BodySans,
                                 body.data(), body.size(), error));
    CHECK(text.set_external_font(nmarkdown::FontRole::BodySansItalic,
                                 italic.data(), italic.size(), error));

    constexpr char sample[] = "affinity wave";
    nmarkdown::GlyphRun regular;
    nmarkdown::GlyphRun native_italic;
    CHECK(text.shape(sample, sizeof(sample) - 1, nmarkdown::fx_from_int(16),
                     regular));
    CHECK(text.shape(sample, sizeof(sample) - 1, nmarkdown::fx_from_int(16),
                     native_italic, nmarkdown::FontRole::BodySans,
                     nmarkdown::TextSpacing::Natural,
                     nmarkdown::FontStyle::Italic));
    CHECK(!regular.glyphs.empty() && !native_italic.glyphs.empty());
    CHECK(std::all_of(regular.glyphs.begin(), regular.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2001;
                      }));
    CHECK(std::all_of(native_italic.glyphs.begin(), native_italic.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2002;
                      }));
    CHECK(native_italic.width != regular.width);

    constexpr int kWidth = 180;
    constexpr int kHeight = 40;
    const std::uint16_t paper = nmarkdown::rgb565(255, 255, 252);
    const std::uint16_t ink = nmarkdown::rgb565(35, 42, 52);
    std::vector<std::uint16_t> native_plain(kWidth * kHeight, paper);
    std::vector<std::uint16_t> native_requested(kWidth * kHeight, paper);
    nmarkdown::Surface565 plain_surface(native_plain.data(), kWidth, kHeight,
                                        kWidth);
    nmarkdown::Surface565 requested_surface(native_requested.data(), kWidth,
                                            kHeight, kWidth);
    CHECK(text.draw_run(plain_surface, native_italic, 6, 26,
                        nmarkdown::fx_from_int(16), ink, paper, false, true,
                        plain_surface.bounds(), nmarkdown::TextSynthesisNone));
    CHECK(text.draw_run(requested_surface, native_italic, 6, 26,
                        nmarkdown::fx_from_int(16), ink, paper, false, true,
                        requested_surface.bounds(),
                        nmarkdown::TextSynthesisItalic));
    // The real oblique face must not be slanted a second time by synthesis.
    CHECK(native_plain == native_requested);

    CHECK(text.set_external_font(nmarkdown::FontRole::BodySansItalic,
                                 nullptr, 0, error));

    nmarkdown::GlyphRun fallback;
    CHECK(text.shape(sample, sizeof(sample) - 1, nmarkdown::fx_from_int(16),
                     fallback, nmarkdown::FontRole::BodySans,
                     nmarkdown::TextSpacing::Natural,
                     nmarkdown::FontStyle::Italic));
    CHECK(std::all_of(fallback.glyphs.begin(), fallback.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2001;
                      }));
    std::vector<std::uint16_t> fallback_regular(kWidth * kHeight, paper);
    std::vector<std::uint16_t> fallback_oblique(kWidth * kHeight, paper);
    nmarkdown::Surface565 fallback_regular_surface(
        fallback_regular.data(), kWidth, kHeight, kWidth);
    nmarkdown::Surface565 fallback_oblique_surface(
        fallback_oblique.data(), kWidth, kHeight, kWidth);
    CHECK(text.draw_run(fallback_regular_surface, fallback, 6, 26,
                        nmarkdown::fx_from_int(16), ink, paper, false, true,
                        fallback_regular_surface.bounds(),
                        nmarkdown::TextSynthesisNone));
    const nmarkdown::GlyphCacheStats regular_stats = text.cache_stats();
    CHECK(text.draw_run(fallback_oblique_surface, fallback, 6, 26,
                        nmarkdown::fx_from_int(16), ink, paper, false, true,
                        fallback_oblique_surface.bounds(),
                        nmarkdown::TextSynthesisItalic));
    const nmarkdown::GlyphCacheStats oblique_stats = text.cache_stats();
    CHECK(fallback_regular != fallback_oblique);
    CHECK(oblique_stats.entries > regular_stats.entries);
}

void test_native_bold_family_faces() {
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    std::vector<std::uint8_t> body;
    std::vector<std::uint8_t> italic;
    CHECK(read_font_asset("DejaVuSans-CX.ttf", body));
    CHECK(read_font_asset("DejaVuSans-Oblique-CX.ttf", italic));
    std::vector<std::uint8_t> bold = body;
    std::vector<std::uint8_t> bold_italic = italic;
    CHECK(mark_font_bold(bold));
    CHECK(mark_font_bold(bold_italic));
    CHECK(text.set_external_font(nmarkdown::FontRole::BodySans,
                                 body.data(), body.size(), error));
    CHECK(text.set_external_font(nmarkdown::FontRole::BodySansBold,
                                 bold.data(), bold.size(), error));
    CHECK(text.set_external_font(nmarkdown::FontRole::BodySansBoldItalic,
                                 bold_italic.data(), bold_italic.size(), error));

    nmarkdown::GlyphRun native_bold;
    CHECK(text.shape("Strong", 6, nmarkdown::fx_from_int(16), native_bold,
                     nmarkdown::FontRole::BodySans,
                     nmarkdown::TextSpacing::Natural,
                     nmarkdown::FontStyle::Bold));
    CHECK(std::all_of(native_bold.glyphs.begin(), native_bold.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2005;
                      }));
    CHECK(!text.requires_synthetic_bold(native_bold));

    nmarkdown::GlyphRun native_bold_italic;
    CHECK(text.shape("Both", 4, nmarkdown::fx_from_int(16),
                     native_bold_italic, nmarkdown::FontRole::BodySans,
                     nmarkdown::TextSpacing::Natural,
                     nmarkdown::FontStyle::BoldItalic));
    CHECK(std::all_of(
        native_bold_italic.glyphs.begin(), native_bold_italic.glyphs.end(),
        [](const nmarkdown::PositionedGlyph& glyph) {
            return glyph.face == 2006;
        }));
    CHECK(!text.requires_synthetic_bold(native_bold_italic));

    constexpr int kWidth = 120;
    constexpr int kHeight = 36;
    const std::uint16_t paper = nmarkdown::rgb565(255, 255, 252);
    const std::uint16_t ink = nmarkdown::rgb565(35, 42, 52);
    std::vector<std::uint16_t> plain(kWidth * kHeight, paper);
    std::vector<std::uint16_t> requested(kWidth * kHeight, paper);
    nmarkdown::Surface565 plain_surface(plain.data(), kWidth, kHeight, kWidth);
    nmarkdown::Surface565 requested_surface(requested.data(), kWidth, kHeight,
                                            kWidth);
    CHECK(text.draw_run(plain_surface, native_bold_italic, 4, 24,
                        nmarkdown::fx_from_int(16), ink, paper, false, true,
                        plain_surface.bounds(), nmarkdown::TextSynthesisNone));
    CHECK(text.draw_run(requested_surface, native_bold_italic, 4, 24,
                        nmarkdown::fx_from_int(16), ink, paper, false, true,
                        requested_surface.bounds(),
                        nmarkdown::TextSynthesisBold |
                            nmarkdown::TextSynthesisItalic));
    CHECK(plain == requested);
}

void test_direct_role_font() {
    std::vector<std::uint8_t> body;
    std::vector<std::uint8_t> monospace;
    std::string error;
    CHECK(read_font_asset("DejaVuSans-CX.ttf", body));
    CHECK(read_font_asset("DejaVuSansMono-CX.ttf", monospace));

    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    CHECK(text.set_external_font(nmarkdown::FontRole::Monospace,
                                 monospace.data(), monospace.size(), error));
    nmarkdown::GlyphRun code;
    CHECK(text.shape("code", 4, nmarkdown::fx_from_int(16), code,
                     nmarkdown::FontRole::Monospace));
    CHECK(std::all_of(code.glyphs.begin(), code.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2003;
                      }));

    CHECK(text.set_external_font(nmarkdown::FontRole::Monospace,
                                 nullptr, 0, error));
    CHECK(text.shape("code", 4, nmarkdown::fx_from_int(16), code,
                     nmarkdown::FontRole::Monospace));
    CHECK(std::all_of(code.glyphs.begin(), code.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2;
                      }));
    nmarkdown::GlyphRun fixed_ascii;
    CHECK(text.shape("MWi0", 4, nmarkdown::fx_from_int(16), fixed_ascii,
                     nmarkdown::FontRole::Monospace));
    CHECK(fixed_ascii.glyphs.size() == 4);
    if (fixed_ascii.glyphs.size() == 4) {
        const nmarkdown::Fx advance = fixed_ascii.glyphs.front().x_advance;
        CHECK(std::all_of(
            fixed_ascii.glyphs.begin(), fixed_ascii.glyphs.end(),
            [advance](const nmarkdown::PositionedGlyph& glyph) {
                return glyph.face == 2 && glyph.x_advance == advance;
            }));
    }

    CHECK(text.set_external_font(nmarkdown::FontRole::Cjk,
                                 body.data(), body.size(), error));
    CHECK(text.shape("code", 4, nmarkdown::fx_from_int(16), code,
                     nmarkdown::FontRole::Monospace));
    CHECK(std::all_of(code.glyphs.begin(), code.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          // The embedded DejaVu Mono ASCII face precedes CJK.
                          return glyph.face == 2;
                      }));
    CHECK(text.shape("code", 4, nmarkdown::fx_from_int(16), code,
                     nmarkdown::FontRole::Cjk));
    CHECK(std::all_of(code.glyphs.begin(), code.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2004;
                      }));
}

void test_shared_font_registry_roles() {
    const std::string path = std::string(NMARKDOWN_SOURCE_DIR) +
                             "/assets/fonts/SarasaFixedSC-Regular-CX.ttf";
    nmarkdown::StdioFileSystem files;
    nmarkdown::DocumentProbe probe;
    std::shared_ptr<nmarkdown::RandomAccessData> source;
    std::string error;
    CHECK(files.probe(path.c_str(), probe, error));
    CHECK(files.open_random_access(path.c_str(), source, error));
    CHECK(source != nullptr);
    if (source == nullptr) return;
    CHECK(source->size() == probe.size);

    nmarkdown::TextSystem text;
    CHECK(text.initialize(error));
    nmarkdown::FontRegistryState registry;
    registry.fonts.push_back({9100, nullptr, source, probe.sample_hash});
    registry.roles[static_cast<std::size_t>(
        nmarkdown::external_font_role_index(
            nmarkdown::FontRole::Monospace))] = 9100;
    registry.roles[static_cast<std::size_t>(
        nmarkdown::external_font_role_index(nmarkdown::FontRole::Cjk))] = 9100;
    nmarkdown::FontRegistryState previous;
    CHECK(text.replace_font_registry(std::move(registry), previous, error));
    CHECK(text.external_font_count() == 1);
    CHECK(text.external_font_bytes() == probe.size);
    CHECK(text.external_font_data(nmarkdown::FontRole::Cjk).empty());
    CHECK(text.external_font_id(nmarkdown::FontRole::Monospace) == 9100);
    CHECK(text.external_font_id(nmarkdown::FontRole::Cjk) == 9100);

    constexpr char cjk[] = u8"中文日本語";
    nmarkdown::GlyphRun code;
    CHECK(text.shape(cjk, sizeof(cjk) - 1, nmarkdown::fx_from_int(15), code,
                     nmarkdown::FontRole::Monospace));
    CHECK(!code.glyphs.empty());
    CHECK(std::all_of(code.glyphs.begin(), code.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 9100;
                      }));
    nmarkdown::GlyphRun fallback;
    CHECK(text.shape(cjk, sizeof(cjk) - 1, nmarkdown::fx_from_int(15),
                     fallback, nmarkdown::FontRole::BodySans));
    CHECK(!fallback.glyphs.empty());
    CHECK(std::all_of(fallback.glyphs.begin(), fallback.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 9100;
                      }));
}

void test_optional_sarasa_fixed_sc() {
    const std::string path = std::string(NMARKDOWN_SOURCE_DIR) +
                             "/assets/fonts/SarasaFixedSC-Regular-CX.ttf";
    std::ifstream input(path, std::ios::binary);
    CHECK(input.good());
    if (!input.good()) return;
    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    CHECK(bytes.size() == 6105504U);

    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    std::vector<std::uint8_t> body;
    std::vector<std::uint8_t> monospace;
    CHECK(read_font_asset("DejaVuSans-CX.ttf", body));
    CHECK(read_font_asset("DejaVuSansMono-CX.ttf", monospace));
    CHECK(text.set_external_font(nmarkdown::FontRole::BodySans,
                                 body.data(), body.size(), error));
    CHECK(text.set_external_font(nmarkdown::FontRole::Monospace,
                                 monospace.data(), monospace.size(), error));
    CHECK(text.set_external_font(nmarkdown::FontRole::Cjk,
                                 bytes.data(), bytes.size(), error));
    nmarkdown::GlyphRun run;
    constexpr char sample[] = u8"中文排版测试，日本語かなカナ。";
    CHECK(text.shape(sample, sizeof(sample) - 1, nmarkdown::fx_from_int(15),
                     run, nmarkdown::FontRole::BodySans));
    CHECK(run.invalid_sequence_count == 0);
    CHECK(run.substitution_count == 0);
    CHECK(std::any_of(run.glyphs.begin(), run.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2004;
                      }));

    nmarkdown::GlyphRun latin_body;
    constexpr char latin_sample[] = "Body fallback";
    CHECK(text.shape(latin_sample, sizeof(latin_sample) - 1,
                     nmarkdown::fx_from_int(15), latin_body,
                     nmarkdown::FontRole::BodySans));
    CHECK(std::all_of(latin_body.glyphs.begin(), latin_body.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2001;
                      }));

    nmarkdown::GlyphRun latin_code;
    constexpr char code_sample[] = "const width = 320;";
    CHECK(text.shape(code_sample, sizeof(code_sample) - 1,
                     nmarkdown::fx_from_int(15), latin_code,
                     nmarkdown::FontRole::Monospace));
    CHECK(std::all_of(latin_code.glyphs.begin(), latin_code.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.face == 2003;
                      }));

    nmarkdown::GlyphRun cjk_cells;
    constexpr char cjk_sample[] = u8"中文日本語";
    CHECK(text.shape(cjk_sample, sizeof(cjk_sample) - 1,
                     nmarkdown::fx_from_int(15), cjk_cells,
                     nmarkdown::FontRole::Cjk));
    CHECK(!cjk_cells.glyphs.empty());
    if (!cjk_cells.glyphs.empty()) {
        const nmarkdown::Fx cjk_advance = cjk_cells.glyphs.front().x_advance;
        CHECK(std::all_of(cjk_cells.glyphs.begin(), cjk_cells.glyphs.end(),
                          [cjk_advance](const nmarkdown::PositionedGlyph& glyph) {
                              return glyph.face == 2004 &&
                                     glyph.x_advance == cjk_advance;
                          }));
    }

    constexpr int kWidth = 180;
    constexpr int kHeight = 40;
    const std::uint16_t paper = nmarkdown::rgb565(255, 255, 252);
    const std::uint16_t ink = nmarkdown::rgb565(35, 42, 52);
    std::vector<std::uint16_t> cjk_regular(kWidth * kHeight, paper);
    std::vector<std::uint16_t> cjk_emphasis(kWidth * kHeight, paper);
    nmarkdown::Surface565 regular_surface(cjk_regular.data(), kWidth, kHeight,
                                          kWidth);
    nmarkdown::Surface565 emphasis_surface(cjk_emphasis.data(), kWidth, kHeight,
                                           kWidth);
    CHECK(text.draw_run(regular_surface, cjk_cells, 6, 26,
                        nmarkdown::fx_from_int(15), ink, paper, false, true,
                        regular_surface.bounds(), nmarkdown::TextSynthesisNone));
    CHECK(text.draw_run(emphasis_surface, cjk_cells, 6, 26,
                        nmarkdown::fx_from_int(15), ink, paper, false, true,
                        emphasis_surface.bounds(),
                        nmarkdown::TextSynthesisItalic));
    CHECK(cjk_regular == cjk_emphasis);
}

void test_shaping_and_fallback() {
    TextFixture fixture;
    CHECK(fixture.initialize());
    if (!fixture.error.empty()) {
        std::fprintf(stderr, "font error: %s\n", fixture.error.c_str());
    }
    nmarkdown::HarfBuzzShaper shaper(fixture.fonts);

    const nmarkdown::GlyphRun multilingual = shape(shaper, u8"AV Ω Ж ∑");
    CHECK(!multilingual.glyphs.empty());
    CHECK(multilingual.invalid_sequence_count == 0);
    CHECK(multilingual.substitution_count == 0);
    CHECK(multilingual.width > 0);

    const nmarkdown::GlyphRun a = shape(shaper, "A");
    const nmarkdown::GlyphRun v = shape(shaper, "V");
    const nmarkdown::GlyphRun av = shape(shaper, "AV");
    CHECK(av.width < a.width + v.width);
    CHECK(av.glyphs.size() == 2);
    if (av.glyphs.size() == 2) {
        CHECK(av.glyphs[0].x_advance <= a.glyphs[0].x_advance);
        CHECK(av.glyphs[1].x_advance == v.glyphs[0].x_advance);
    }
    const nmarkdown::GlyphRun tracked = shape(
        shaper,
        "AV",
        16,
        nmarkdown::FontRole::BodySans,
        nmarkdown::TextSpacing::Tracked);
    CHECK(tracked.glyphs.size() == 2);
    CHECK(tracked.width == av.width + nmarkdown::fx_from_int(1));
    const nmarkdown::GlyphRun tracked_single = shape(
        shaper,
        "A",
        16,
        nmarkdown::FontRole::BodySans,
        nmarkdown::TextSpacing::Tracked);
    CHECK(tracked_single.width == a.width);

    if (!a.glyphs.empty()) {
        const nmarkdown::FontFace* face = fixture.fonts.face(a.glyphs[0].face);
        nmarkdown::GlyphMetrics metrics;
        CHECK(face != nullptr);
        if (face != nullptr) {
            CHECK(face->glyph_metrics(a.glyphs[0].glyph,
                                      nmarkdown::fx_from_int(16), metrics));
            CHECK(std::abs(a.glyphs[0].x_advance - metrics.advance) <= 1);
        }
    }

    const nmarkdown::GlyphRun combining = shape(shaper, "e\xCC\x81");
    CHECK(!combining.glyphs.empty());
    CHECK(std::all_of(combining.glyphs.begin(), combining.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.source_cluster == 0;
                      }));
    CHECK(std::any_of(combining.glyphs.begin(), combining.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.combining_mark;
                      }));

    // Long runs guard the cluster bookkeeping against an accidental return
    // to a per-glyph linear scan.
    const std::string long_run_source(8192, 'a');
    nmarkdown::GlyphRun long_run;
    CHECK(shaper.shape(
        reinterpret_cast<const std::uint8_t*>(long_run_source.data()),
        long_run_source.size(), nmarkdown::fx_from_int(16), long_run));
    CHECK(long_run.glyphs.size() == long_run_source.size());

    const nmarkdown::GlyphRun ligature = shape(shaper, "ffi");
    CHECK(ligature.glyphs.size() == 1);
    CHECK(!ligature.glyphs.empty() &&
          ligature.glyphs.front().source_cluster == 0);

    const nmarkdown::GlyphRun cjk = shape(shaper, u8"中");
    CHECK(cjk.substitution_count == 1);
    CHECK(cjk.glyphs.size() == 1);
    if (!cjk.glyphs.empty()) {
        CHECK(cjk.glyphs.front().codepoint == 0xFFFDU);
    }

    const nmarkdown::GlyphRun complex = shape(shaper, u8"العربية");
    CHECK(complex.unsupported_script_count == 0);
    CHECK(complex.substitution_count == 7);
    CHECK(std::all_of(complex.glyphs.begin(), complex.glyphs.end(),
                      [](const nmarkdown::PositionedGlyph& glyph) {
                          return glyph.codepoint == 0xFFFDU;
                      }));

    const char malformed[] = {'x', static_cast<char>(0xE2), '(', 0};
    const nmarkdown::GlyphRun invalid = shape(shaper, malformed);
    CHECK(invalid.invalid_sequence_count == 1);
    CHECK(invalid.glyphs.size() == 3);
}

void test_compositor_and_cache() {
    TextFixture fixture;
    CHECK(fixture.initialize());
    nmarkdown::HarfBuzzShaper shaper(fixture.fonts);
    const nmarkdown::GlyphRun run = shape(shaper, u8"Latin Ωμέγα Жизнь → ∑∞");

    constexpr int kWidth = 320;
    constexpr int kHeight = 80;
    const std::uint16_t light_background = nmarkdown::rgb565(255, 255, 252);
    const std::uint16_t dark_background = nmarkdown::rgb565(24, 29, 38);
    const std::uint16_t dark_text = nmarkdown::rgb565(35, 42, 52);
    const std::uint16_t light_text = nmarkdown::rgb565(239, 242, 247);
    std::vector<std::uint16_t> pixels(kWidth * kHeight, light_background);
    nmarkdown::Surface565 surface(pixels.data(), kWidth, kHeight, kWidth);

    nmarkdown::GlyphCache cache;
    nmarkdown::TextRenderer renderer(fixture.fonts, cache);
    const nmarkdown::CoverageLut light_lut =
        nmarkdown::make_light_coverage_lut(36, 160);
    CHECK(renderer.draw_run(surface,
                            run,
                            8,
                            30,
                            nmarkdown::fx_from_int(16),
                            dark_text,
                            light_background,
                            light_lut,
                            true,
                            surface.bounds()));
    const auto changed = std::count_if(pixels.begin(), pixels.end(), [&](std::uint16_t pixel) {
        return pixel != light_background;
    });
    CHECK(changed > 100);
    const nmarkdown::GlyphCacheStats first = cache.stats();
    CHECK(first.misses > 10);

    CHECK(renderer.draw_run(surface,
                            run,
                            8,
                            60,
                            nmarkdown::fx_from_int(16),
                            dark_text,
                            light_background,
                            light_lut,
                            true,
                            surface.bounds()));
    const nmarkdown::GlyphCacheStats second = cache.stats();
    CHECK(second.hits > first.hits);
    CHECK(second.misses == first.misses);

    surface.clear(dark_background);
    CHECK(renderer.draw_run(surface,
                            run,
                            8,
                            30,
                            nmarkdown::fx_from_int(16),
                            light_text,
                            dark_background,
                            nmarkdown::make_dark_coverage_lut(20),
                            true,
                            surface.bounds()));
    CHECK(std::count(pixels.begin(), pixels.end(), dark_background) <
          static_cast<std::ptrdiff_t>(pixels.size()));

    cache.clear();
    const nmarkdown::GlyphRun fractional = shape(shaper, "iiiiiiii", 15);
    surface.clear(light_background);
    CHECK(renderer.draw_run(surface,
                            fractional,
                            8,
                            30,
                            nmarkdown::fx_from_int(15),
                            dark_text,
                            light_background,
                            light_lut,
                            true,
                            surface.bounds()));
    // The same glyph is cached at more than one quarter-pixel phase.  This
    // prevents fractional HarfBuzz advances from becoming visibly uneven
    // whole-pixel jumps on the calculator display.
    CHECK(cache.stats().entries > 1);
    CHECK(cache.stats().entries <= 4);
}

void test_page_lru_eviction() {
    TextFixture fixture;
    CHECK(fixture.initialize());
    nmarkdown::GlyphCache cache(1);
    for (std::uint32_t codepoint = 33; codepoint < 0x3000 &&
                                         cache.stats().evictions == 0;
         ++codepoint) {
        nmarkdown::ResolvedGlyph resolved;
        if (!fixture.fonts.resolve(codepoint, resolved) || resolved.substituted) {
            continue;
        }
        nmarkdown::GlyphCacheHandle handle;
        CHECK(cache.get(*resolved.face,
                        resolved.glyph,
                        nmarkdown::fx_from_int(32),
                        0,
                        0,
                        handle));
    }
    CHECK(cache.stats().evictions > 0);
    CHECK(cache.stats().pages == 1);
}

void test_cached_run_detects_page_lru_eviction() {
    TextFixture fixture;
    CHECK(fixture.initialize());
    nmarkdown::HarfBuzzShaper shaper(fixture.fonts);
    nmarkdown::GlyphCache cache(1);
    nmarkdown::TextRenderer renderer(fixture.fonts, cache);
    const nmarkdown::GlyphRun target = shape(shaper, "A", 32);
    CHECK(renderer.cache_run(
              target, nmarkdown::fx_from_int(32), target.glyphs.size()) ==
          target.glyphs.size());
    CHECK(renderer.run_cached(target, nmarkdown::fx_from_int(32)));

    for (std::uint32_t codepoint = 33;
         codepoint < 0x3000 && cache.stats().evictions == 0;
         ++codepoint) {
        nmarkdown::ResolvedGlyph resolved;
        if (!fixture.fonts.resolve(codepoint, resolved) ||
            resolved.substituted) {
            continue;
        }
        nmarkdown::GlyphCacheHandle handle;
        CHECK(cache.get(*resolved.face,
                        resolved.glyph,
                        nmarkdown::fx_from_int(32),
                        0,
                        nmarkdown::GlyphRenderNone,
                        handle));
    }
    CHECK(cache.stats().evictions > 0);
    CHECK(!renderer.run_cached(target, nmarkdown::fx_from_int(32)));
}

void test_blend_endpoints() {
    const std::uint16_t black = nmarkdown::rgb565(0, 0, 0);
    const std::uint16_t white = nmarkdown::rgb565(255, 255, 255);
    CHECK(nmarkdown::blend565(white, black, 0) == white);
    CHECK(nmarkdown::blend565(white, black, 255) == black);
    const std::uint16_t middle = nmarkdown::blend565(white, black, 128);
    const nmarkdown::CoverageLut palette_lut = nmarkdown::make_coverage_lut(20);
    const nmarkdown::CoveragePalette565 palette =
        nmarkdown::make_coverage_palette565(black, white, palette_lut);
    CHECK(palette[0] == white);
    CHECK(palette[255] == black);
    CHECK(palette[128] == nmarkdown::blend565(white, black, palette_lut[128]));
    CHECK(middle != white && middle != black);
}

void test_dark_coverage_is_clear_and_monotonic() {
    const nmarkdown::CoverageLut light = nmarkdown::make_coverage_lut(20);
    const nmarkdown::CoverageLut dark = nmarkdown::make_dark_coverage_lut(20);
    CHECK(dark[0] == 0);
    CHECK(dark[255] == 255);
    for (std::size_t index = 1; index < dark.size(); ++index) {
        CHECK(dark[index] >= dark[index - 1]);
        CHECK(dark[index] >= light[index]);
    }

    // Preserve the quiet fringe, but materially brighten the mid-coverage
    // pixels that otherwise disappear after 5/6-bit RGB quantization.
    CHECK(dark[1] == 1);
    CHECK(dark[128] >= 175 && dark[128] <= 185);
    CHECK(dark[128] >= light[128] + 40);
    const std::uint16_t black = nmarkdown::rgb565(0, 0, 0);
    const std::uint16_t white = nmarkdown::rgb565(255, 255, 255);
    const std::uint16_t old_edge = nmarkdown::blend565(
        black, white, light[128]);
    const std::uint16_t corrected_edge = nmarkdown::blend565(
        black, white, dark[128]);
    CHECK(nmarkdown::red8(corrected_edge) >= nmarkdown::red8(old_edge) + 40);
    CHECK(nmarkdown::green8(corrected_edge) >=
          nmarkdown::green8(old_edge) + 40);
    CHECK(nmarkdown::blue8(corrected_edge) >= nmarkdown::blue8(old_edge) + 40);
}

void test_light_coverage_is_sharp_and_monotonic() {
    const nmarkdown::CoverageLut neutral = nmarkdown::make_coverage_lut(36);
    const nmarkdown::CoverageLut disabled =
        nmarkdown::make_light_coverage_lut(36, 0);
    const nmarkdown::CoverageLut sharp =
        nmarkdown::make_light_coverage_lut(36, 160);
    CHECK(disabled == neutral);
    CHECK(sharp[0] == 0);
    CHECK(sharp[255] == 255);
    for (std::size_t index = 1; index < sharp.size(); ++index) {
        CHECK(sharp[index] >= sharp[index - 1]);
    }

    // Remove the pale, quantized fringe without thinning the dark half of a
    // hinted stem.  The midpoint remains stable, while high-coverage pixels
    // gain enough contrast to survive the 5/6-bit framebuffer conversion.
    CHECK(sharp[1] == 1);
    CHECK(sharp[16] <= neutral[16] - 8);
    CHECK(sharp[64] <= neutral[64] - 12);
    CHECK(sharp[128] == neutral[128]);
    CHECK(sharp[192] >= neutral[192] + 12);
    CHECK(sharp[240] >= neutral[240] + 7);

    const std::uint16_t black = nmarkdown::rgb565(0, 0, 0);
    const std::uint16_t white = nmarkdown::rgb565(255, 255, 255);
    const std::uint16_t neutral_fringe = nmarkdown::blend565(
        white, black, neutral[64]);
    const std::uint16_t sharp_fringe = nmarkdown::blend565(
        white, black, sharp[64]);
    const std::uint16_t neutral_stem = nmarkdown::blend565(
        white, black, neutral[192]);
    const std::uint16_t sharp_stem = nmarkdown::blend565(
        white, black, sharp[192]);
    CHECK(nmarkdown::red8(sharp_fringe) > nmarkdown::red8(neutral_fringe));
    CHECK(nmarkdown::green8(sharp_fringe) > nmarkdown::green8(neutral_fringe));
    CHECK(nmarkdown::blue8(sharp_fringe) > nmarkdown::blue8(neutral_fringe));
    CHECK(nmarkdown::red8(sharp_stem) < nmarkdown::red8(neutral_stem));
    CHECK(nmarkdown::green8(sharp_stem) < nmarkdown::green8(neutral_stem));
    CHECK(nmarkdown::blue8(sharp_stem) < nmarkdown::blue8(neutral_stem));
}

void test_render_sharpness_range_without_reloading_fonts() {
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    CHECK(text.render_sharpness() == nmarkdown::kDefaultRenderSharpness);
    const std::uint32_t signature = text.font_signature();
    const nmarkdown::CoverageLut balanced_light = text.coverage_lut(false);
    const nmarkdown::CoverageLut balanced_dark = text.coverage_lut(true);

    text.set_render_sharpness(nmarkdown::kMaximumRenderSharpness);
    const nmarkdown::CoverageLut sharp_light = text.coverage_lut(false);
    const nmarkdown::CoverageLut sharp_dark = text.coverage_lut(true);
    const auto interpolate = [](const nmarkdown::CoverageLut& smooth,
                                const nmarkdown::CoverageLut& sharp,
                                unsigned numerator,
                                unsigned denominator) {
        nmarkdown::CoverageLut result{};
        const unsigned inverse = denominator - numerator;
        for (std::size_t alpha = 0; alpha < result.size(); ++alpha) {
            result[alpha] = static_cast<std::uint8_t>(
                (static_cast<unsigned>(smooth[alpha]) * inverse +
                 static_cast<unsigned>(sharp[alpha]) * numerator +
                 denominator / 2U) /
                denominator);
        }
        result[0] = 0;
        result[255] = 255;
        return result;
    };
    nmarkdown::CoverageLut near_binary{};
    for (unsigned alpha = 0; alpha < near_binary.size(); ++alpha) {
        if (alpha <= 88U) near_binary[alpha] = 0;
        else if (alpha >= 168U) near_binary[alpha] = 255;
        else near_binary[alpha] = static_cast<std::uint8_t>(
            (alpha - 88U) * 255U / 80U);
    }
    const nmarkdown::CoverageLut old_balanced_light = interpolate(
        nmarkdown::make_coverage_lut(20),
        nmarkdown::make_light_coverage_lut(36, 160), 1, 2);
    const nmarkdown::CoverageLut old_balanced_dark = interpolate(
        nmarkdown::make_dark_coverage_lut(20),
        nmarkdown::make_light_coverage_lut(20, 112), 1, 2);
    CHECK(sharp_light ==
          interpolate(old_balanced_light, near_binary, 2, 4));
    CHECK(sharp_dark ==
          interpolate(old_balanced_dark, near_binary, 2, 4));

    text.set_render_sharpness(nmarkdown::kMinimumRenderSharpness);
    CHECK(text.render_sharpness() == nmarkdown::kMinimumRenderSharpness);
    const nmarkdown::CoverageLut smooth_light = text.coverage_lut(false);
    const nmarkdown::CoverageLut smooth_dark = text.coverage_lut(true);
    const nmarkdown::CoverageLut old_neutral_light =
        nmarkdown::make_coverage_lut(0);
    const nmarkdown::CoverageLut old_neutral_dark =
        nmarkdown::make_dark_coverage_lut(0);
    CHECK(smooth_light != sharp_light);
    CHECK(smooth_dark != sharp_dark);
    CHECK(balanced_light != smooth_light && balanced_light != sharp_light);
    CHECK(balanced_dark != smooth_dark && balanced_dark != sharp_dark);
    CHECK(smooth_light[64] > sharp_light[64]);
    CHECK(smooth_dark[64] > sharp_dark[64]);
    CHECK(smooth_light[64] > old_neutral_light[64]);
    CHECK(smooth_light[192] < old_neutral_light[192]);
    CHECK(smooth_dark[64] > old_neutral_dark[64]);
    CHECK(smooth_dark[192] < old_neutral_dark[192]);
    CHECK(balanced_light == interpolate(
        smooth_light, sharp_light, nmarkdown::kDefaultRenderSharpness,
        nmarkdown::kMaximumRenderSharpness));
    CHECK(balanced_dark == interpolate(
        smooth_dark, sharp_dark, nmarkdown::kDefaultRenderSharpness,
        nmarkdown::kMaximumRenderSharpness));

    nmarkdown::CoverageLut previous_light = smooth_light;
    nmarkdown::CoverageLut previous_dark = smooth_dark;
    for (unsigned level = 0; level <= nmarkdown::kMaximumRenderSharpness;
         ++level) {
        text.set_render_sharpness(static_cast<nmarkdown::RenderSharpness>(level));
        const nmarkdown::CoverageLut light = text.coverage_lut(false);
        const nmarkdown::CoverageLut dark = text.coverage_lut(true);
        CHECK(light[0] == 0 && light[255] == 255);
        CHECK(dark[0] == 0 && dark[255] == 255);
        for (std::size_t alpha = 1; alpha < light.size(); ++alpha) {
            CHECK(light[alpha] >= light[alpha - 1]);
            CHECK(dark[alpha] >= dark[alpha - 1]);
        }
        if (level != 0) {
            CHECK(light != previous_light);
            CHECK(dark != previous_dark);
        }
        previous_light = light;
        previous_dark = dark;
    }
    CHECK(text.coverage_lut(false) == sharp_light);
    CHECK(text.coverage_lut(true) == sharp_dark);
    CHECK(text.font_signature() == signature);
}

void test_default_glyph_cache_has_large_lazy_capacity() {
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    CHECK(error.empty());
    CHECK(text.cache_stats().pages == 128);
    CHECK(text.cache_stats().entries == 0);
}

void test_block_cached_random_access_coalesces_tiny_font_reads() {
    std::vector<std::uint8_t> bytes(16U * 1024U);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(
            (index * 37U + 11U) & 0xFFU);
    }
    auto source =
        std::make_shared<CountingRandomAccess>(std::move(bytes));
    std::shared_ptr<nmarkdown::RandomAccessData> cached =
        nmarkdown::make_block_cached_random_access(
            source, 4U * 512U, 512U);
    CHECK(cached != nullptr);

    std::array<std::uint8_t, 64> result{};
    CHECK(cached->read(100, result.data(), 32));
    CHECK(source->read_calls == 1);
    CHECK(source->bytes_read == 512);
    CHECK(cached->read(120, result.data(), 24));
    CHECK(source->read_calls == 1);

    // A request crossing the cached block boundary fetches only the missing
    // neighboring block, then both halves remain resident.
    CHECK(cached->read(500, result.data(), 32));
    CHECK(source->read_calls == 2);
    CHECK(source->bytes_read == 1024);
    CHECK(cached->read(496, result.data(), 40));
    CHECK(source->read_calls == 2);

    // Large table loads bypass the tiny-block path as one underlying read.
    std::vector<std::uint8_t> table(2048);
    CHECK(cached->read(4096, table.data(), table.size()));
    CHECK(source->read_calls == 3);
    CHECK(source->bytes_read == 3072);
}

void test_block_cache_does_not_expose_partially_failed_fill() {
    std::vector<std::uint8_t> bytes(1024);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(index & 0xFFU);
    }
    auto source =
        std::make_shared<PartiallyFailingRandomAccess>(
            std::move(bytes));
    nmarkdown::BlockCachedRandomAccessData cached(
        source, 512, 512);
    std::array<std::uint8_t, 32> result{};
    CHECK(cached.read(32, result.data(), result.size()));
    CHECK(result[0] == 32);

    source->fail_next = true;
    CHECK(!cached.read(512, result.data(), result.size()));
    const std::size_t calls_after_failure = source->read_calls;
    CHECK(cached.read(32, result.data(), result.size()));
    CHECK(source->read_calls == calls_after_failure + 1);
    CHECK(result[0] == 32);
    CHECK(result[31] == 63);
}

void test_streamed_freetype_face_reuses_cached_glyph_blocks() {
    std::vector<std::uint8_t> bytes;
    CHECK(read_font_asset("SarasaFixedSC-Regular-CX.ttf", bytes));
    if (bytes.empty()) return;

    auto source =
        std::make_shared<CountingRandomAccess>(std::move(bytes));
    nmarkdown::FontPack pack;
    std::string error;
    CHECK(pack.load_from_memory(
        nmarkdown::kCoreFontPack,
        nmarkdown::kCoreFontPackSize, error));
    nmarkdown::FontCollection fonts;
    const std::vector<nmarkdown::MemoryFontFace> external = {
        {9300, nmarkdown::FontRole::Cjk, nullptr, 0, source},
    };
    CHECK(fonts.initialize(pack, external, error));
    const nmarkdown::FontFace* face = fonts.face(9300);
    CHECK(face != nullptr);
    if (face == nullptr) return;

    nmarkdown::GlyphId glyph = 0;
    CHECK(face->glyph_for_codepoint(0x4E2DU, glyph));
    CHECK(glyph != 0);
    const std::size_t reads_before_first = source->read_calls;
    nmarkdown::GlyphBitmap first;
    CHECK(face->rasterize(
        glyph, nmarkdown::fx_from_int(15), 0,
        nmarkdown::GlyphRenderNone, first));
    const std::size_t reads_after_first = source->read_calls;
    CHECK(reads_after_first != 0);
    CHECK(reads_after_first - reads_before_first <= 4);

    // FreeType itself reloads an outline on every FT_Load_Glyph call. The
    // custom stream cache must keep a repeated rasterization from issuing
    // another storage-backed hmtx/glyf transaction.
    nmarkdown::GlyphBitmap second;
    CHECK(face->rasterize(
        glyph, nmarkdown::fx_from_int(15), 0,
        nmarkdown::GlyphRenderNone, second));
    CHECK(source->read_calls == reads_after_first);
    CHECK(second.metrics.width == first.metrics.width);
    CHECK(second.metrics.height == first.metrics.height);
}

}  // namespace

int main() {
    test_shaping_and_fallback();
    test_role_aware_shaping();
    test_native_and_outline_fallback_italics();
    test_native_bold_family_faces();
    test_direct_role_font();
    test_shared_font_registry_roles();
    test_optional_sarasa_fixed_sc();
    test_compositor_and_cache();
    test_page_lru_eviction();
    test_cached_run_detects_page_lru_eviction();
    test_blend_endpoints();
    test_dark_coverage_is_clear_and_monotonic();
    test_light_coverage_is_sharp_and_monotonic();
    test_render_sharpness_range_without_reloading_fonts();
    test_default_glyph_cache_has_large_lazy_capacity();
    test_block_cached_random_access_coalesces_tiny_font_reads();
    test_block_cache_does_not_expose_partially_failed_fill();
    test_streamed_freetype_face_reuses_cached_glyph_blocks();
    if (failures != 0) {
        std::fprintf(stderr, "%d text test(s) failed\n", failures);
        return 1;
    }
    std::printf("All text tests passed\n");
    return 0;
}
