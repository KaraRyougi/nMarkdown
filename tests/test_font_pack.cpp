#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "nmarkdown/generated/core_font_pack.h"
#include "nmarkdown/text/harfbuzz_shaper.h"
#include "nmarkdown/text/font_pack.h"
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

bool contains_ascii(const std::uint8_t* bytes,
                    std::size_t size,
                    const char* text) {
    const std::size_t text_size = std::char_traits<char>::length(text);
    return std::search(bytes, bytes + size, text, text + text_size) !=
           bytes + size;
}

void test_embedded_pack() {
    nmarkdown::FontPack pack;
    std::string error;
    CHECK(pack.load_from_memory(nmarkdown::kCoreFontPack,
                                nmarkdown::kCoreFontPackSize,
                                error));
    CHECK(error.empty());
    CHECK(pack.valid());
    CHECK(pack.face_count() == 3);
    const nmarkdown::FontPackFace* face = pack.face(0);
    CHECK(face != nullptr);
    if (face != nullptr) {
        CHECK(face->id == 1);
        CHECK(face->role == nmarkdown::FontRole::BodySans);
        CHECK(face->name == "DejaVu Sans UI");
        CHECK(face->font_size > 25000 && face->font_size < 27000);
        // Runtime attribution remains in THIRD_PARTY_NOTICES.md rather than
        // duplicating copyright and license strings in the bootstrap face.
        CHECK(face->license.empty());
        CHECK(!contains_ascii(face->font_data, face->font_size,
                              "Bitstream Vera Fonts Copyright"));
        CHECK(face->declares_codepoint('A'));
        CHECK(!face->declares_codepoint(0x00E9U));
        CHECK(!face->declares_codepoint(0x03A9U));
        CHECK(!face->declares_codepoint(0x0416U));
        CHECK(!face->declares_codepoint(0x2610U));
        CHECK(!face->declares_codepoint(0x2611U));
        CHECK(face->declares_codepoint(0xFFFDU));
        CHECK(!face->declares_codepoint(0x4E00U));
    }
    const nmarkdown::FontPackFace* monospace = pack.face_by_id(2);
    CHECK(monospace != nullptr);
    if (monospace != nullptr) {
        CHECK(monospace->role == nmarkdown::FontRole::Monospace);
        CHECK(monospace->name == "DejaVu Sans Mono UI");
        CHECK(monospace->font_size > 17000 && monospace->font_size < 18000);
        CHECK(monospace->license.empty());
        CHECK(!contains_ascii(monospace->font_data, monospace->font_size,
                              "Bitstream Vera Fonts Copyright"));
        CHECK(monospace->declares_codepoint('M'));
        CHECK(monospace->declares_codepoint(0xFFFDU));
        CHECK(!monospace->declares_codepoint(0x4E00U));
    }
    CHECK(pack.face_by_id(4) == nullptr);
    const nmarkdown::FontPackFace* math = pack.face(2);
    CHECK(math != nullptr);
    if (math != nullptr) {
        CHECK(math->id == 3);
        CHECK(math->role == nmarkdown::FontRole::Math);
        CHECK(math->name == "Latin Modern Math");
        CHECK(math->font_size > 700000 && math->font_size < 800000);
        CHECK(math->license.empty());
        CHECK(contains_ascii(math->font_data, math->font_size,
                             "GUST Font License"));
        CHECK(math->declares_codepoint('{'));
        CHECK(math->declares_codepoint('}'));
        CHECK(math->declares_codepoint('|'));
        CHECK(math->declares_codepoint(0x221AU));
        CHECK(math->declares_codepoint(0x1D465U));
    }
}

void test_corruption_rejected() {
    std::vector<std::uint8_t> corrupt(nmarkdown::kCoreFontPack,
                                      nmarkdown::kCoreFontPack +
                                          nmarkdown::kCoreFontPackSize);
    corrupt.back() ^= 0x01U;
    nmarkdown::FontPack pack;
    std::string error;
    CHECK(!pack.load_from_memory(corrupt.data(), corrupt.size(), error));
    CHECK(error.find("checksum") != std::string::npos);

    CHECK(!pack.load_from_memory(corrupt.data(), 12, error));
    CHECK(!error.empty());
}

void test_external_monospace_cjk_pack(const char* path) {
    nmarkdown::FontPack pack;
    std::string error;
    CHECK(pack.load_from_file(path, error));
    CHECK(error.empty());
    const nmarkdown::FontPackFace* face = pack.face(0);
    CHECK(face != nullptr);
    if (face == nullptr) return;
    CHECK(face->role == nmarkdown::FontRole::Monospace);
    CHECK(face->license.empty());

    nmarkdown::FontCollection fonts;
    CHECK(fonts.initialize(pack, error));
    nmarkdown::HarfBuzzShaper shaper(fonts);
    nmarkdown::GlyphRun monospace;
    constexpr char code[] = "MWi0";
    CHECK(shaper.shape(reinterpret_cast<const std::uint8_t*>(code),
                       sizeof(code) - 1, nmarkdown::fx_from_int(14),
                       monospace, nmarkdown::FontRole::Monospace));
    CHECK(monospace.substitution_count == 0);
    CHECK(monospace.glyphs.size() == sizeof(code) - 1);
    if (!monospace.glyphs.empty()) {
        const nmarkdown::Fx advance = monospace.glyphs.front().x_advance;
        for (const nmarkdown::PositionedGlyph& glyph : monospace.glyphs) {
            CHECK(glyph.face == face->id);
            CHECK(glyph.x_advance == advance);
        }
    }

    nmarkdown::GlyphRun cjk;
    constexpr char sample[] = u8"中文日本語かなカナ";
    CHECK(shaper.shape(reinterpret_cast<const std::uint8_t*>(sample),
                       sizeof(sample) - 1, nmarkdown::fx_from_int(15), cjk));
    CHECK(cjk.substitution_count == 0);
    CHECK(cjk.glyphs.size() == 9);
    nmarkdown::GlyphRun fixed_latin;
    CHECK(shaper.shape(reinterpret_cast<const std::uint8_t*>(code),
                       sizeof(code) - 1, nmarkdown::fx_from_int(15),
                       fixed_latin, nmarkdown::FontRole::Monospace));
    const nmarkdown::Fx cjk_advance = cjk.glyphs.empty()
                                         ? 0
                                         : cjk.glyphs.front().x_advance;
    const nmarkdown::Fx latin_advance = fixed_latin.glyphs.empty()
                                           ? 0
                                           : fixed_latin.glyphs.front().x_advance;
    for (const nmarkdown::PositionedGlyph& glyph : cjk.glyphs) {
        CHECK(glyph.face == face->id);
        CHECK(glyph.x_advance == cjk_advance);
    }
    for (const nmarkdown::PositionedGlyph& glyph : fixed_latin.glyphs) {
        CHECK(glyph.face == face->id);
        CHECK(glyph.x_advance == latin_advance);
    }
    // Sarasa Fixed uses half-em Latin and full-em CJK cells.  This verifies
    // HarfBuzz preserves those uniform design advances without inserting any
    // extra reader-side spacing.
    CHECK(cjk_advance == latin_advance * 2);
}

}  // namespace

int main(int argc, char** argv) {
    test_embedded_pack();
    test_corruption_rejected();
    if (argc > 1) test_external_monospace_cjk_pack(argv[1]);
    if (failures != 0) {
        std::fprintf(stderr, "%d font-pack test(s) failed\n", failures);
        return 1;
    }
    std::printf("All font-pack tests passed\n");
    return 0;
}
