#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "nmarkdown/document/state.h"

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

std::uint32_t state_checksum(const std::uint8_t* bytes, std::size_t size) {
    std::uint32_t hash = 2166136261U;
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

void test_round_trip_and_corruption() {
    const std::uint8_t document[] = {'a', 'b', 'c'};
    const nmarkdown::ReaderState defaults;
    CHECK(defaults.code_wrap);
    CHECK(defaults.natural_scrolling);
    CHECK(defaults.natural_swiping);
    CHECK(defaults.reading_mode == nmarkdown::ReadingMode::VerticalScroll);
    CHECK(defaults.render_sharpness == nmarkdown::kDefaultRenderSharpness);
    nmarkdown::ReaderState input;
    input.position.document_identity = nmarkdown::document_identity(document, 3);
    input.position.source_offset = 1234;
    input.position.nearest_block = 42;
    input.position.relative_position_0_65535 = 32768;
    input.bookmarks = {4, 99, 1024};
    input.last_selected_heading = 7;
    input.font_size = 18;
    input.line_gap = 7;
    input.side_margin = 11;
    input.dark_theme = true;
    input.high_contrast = true;
    input.code_wrap = true;
    input.table_mode = 1;
    input.reading_mode = nmarkdown::ReadingMode::HorizontalScroll;
    input.natural_scrolling = false;
    input.natural_swiping = false;
    input.render_sharpness = 3;
    std::vector<std::uint8_t> bytes;
    std::string error;
    CHECK(nmarkdown::encode_reader_state(input, bytes, error));
    nmarkdown::ReaderState output;
    CHECK(nmarkdown::decode_reader_state(bytes.data(), bytes.size(), output, error));
    CHECK(output.position.document_identity == input.position.document_identity);
    CHECK(output.position.source_offset == 1234);
    CHECK(output.position.nearest_block == 42);
    CHECK(output.position.relative_position_0_65535 == 32768);
    CHECK(output.bookmarks == input.bookmarks);
    CHECK(output.last_selected_heading == 7);
    CHECK(output.font_size == 18 && output.dark_theme && output.code_wrap);
    CHECK(output.line_gap == 7);
    CHECK(output.side_margin == 11);
    CHECK(output.high_contrast);
    CHECK(output.table_mode == 1);
    CHECK(output.reading_mode == nmarkdown::ReadingMode::HorizontalScroll);
    CHECK(!output.natural_scrolling);
    CHECK(!output.natural_swiping);
    CHECK(output.render_sharpness == 3);

    nmarkdown::ReaderState automatic_input;
    automatic_input.line_gap = 0;
    std::vector<std::uint8_t> automatic_bytes;
    CHECK(nmarkdown::encode_reader_state(automatic_input, automatic_bytes, error));
    nmarkdown::ReaderState automatic_output;
    CHECK(nmarkdown::decode_reader_state(automatic_bytes.data(), automatic_bytes.size(),
                                         automatic_output, error));
    CHECK(automatic_output.line_gap == 0);
    CHECK(automatic_output.natural_scrolling);
    CHECK(automatic_output.natural_swiping);
    CHECK(automatic_output.render_sharpness ==
          nmarkdown::kDefaultRenderSharpness);

    // Version 3 reserved only bit 0. It must not silently reinterpret the new
    // Natural-scrolling bit without the explicit version-4 schema boundary.
    std::vector<std::uint8_t> invalid_version3 = automatic_bytes;
    invalid_version3[4] = 3;
    invalid_version3[5] = 0;
    std::uint32_t invalid_checksum =
        state_checksum(invalid_version3.data(), invalid_version3.size() - 4);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        invalid_version3[invalid_version3.size() - 4 + shift / 8] =
            static_cast<std::uint8_t>(invalid_checksum >> shift);
    }
    CHECK(!nmarkdown::decode_reader_state(
        invalid_version3.data(), invalid_version3.size(), output, error));

    std::vector<std::uint8_t> unknown_flags = automatic_bytes;
    unknown_flags[39] = static_cast<std::uint8_t>(
        (unknown_flags[39] & 3U) | (11U << 2U));
    invalid_checksum =
        state_checksum(unknown_flags.data(), unknown_flags.size() - 4);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        unknown_flags[unknown_flags.size() - 4 + shift / 8] =
            static_cast<std::uint8_t>(invalid_checksum >> shift);
    }
    CHECK(!nmarkdown::decode_reader_state(
        unknown_flags.data(), unknown_flags.size(), output, error));

    std::vector<std::uint8_t> version3 = bytes;
    version3[4] = 3;
    version3[5] = 0;
    version3[39] &= 1U;
    std::uint32_t legacy_checksum =
        state_checksum(version3.data(), version3.size() - 4);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        version3[version3.size() - 4 + shift / 8] =
            static_cast<std::uint8_t>(legacy_checksum >> shift);
    }
    CHECK(nmarkdown::decode_reader_state(
        version3.data(), version3.size(), output, error));
    CHECK(output.reading_mode == nmarkdown::ReadingMode::HorizontalScroll);
    CHECK(output.natural_scrolling);
    CHECK(output.natural_swiping);
    CHECK(output.render_sharpness == nmarkdown::kMaximumRenderSharpness);

    std::vector<std::uint8_t> version4 = automatic_bytes;
    version4[4] = 4;
    version4[5] = 0;
    version4[39] &= 3U;
    legacy_checksum = state_checksum(version4.data(), version4.size() - 4);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        version4[version4.size() - 4 + shift / 8] =
            static_cast<std::uint8_t>(legacy_checksum >> shift);
    }
    CHECK(nmarkdown::decode_reader_state(
        version4.data(), version4.size(), output, error));
    CHECK(output.natural_scrolling);
    CHECK(output.natural_swiping);
    CHECK(output.render_sharpness == nmarkdown::kMaximumRenderSharpness);

    std::vector<std::uint8_t> version5 = automatic_bytes;
    version5[4] = 5;
    version5[5] = 0;
    version5[39] = static_cast<std::uint8_t>(
        (version5[39] & 3U) | 4U);
    legacy_checksum = state_checksum(version5.data(), version5.size() - 4);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        version5[version5.size() - 4 + shift / 8] =
            static_cast<std::uint8_t>(legacy_checksum >> shift);
    }
    CHECK(nmarkdown::decode_reader_state(
        version5.data(), version5.size(), output, error));
    CHECK(output.render_sharpness == nmarkdown::kMinimumRenderSharpness);
    CHECK(output.natural_swiping);

    // Version 6 predates the independent discrete-swipe direction. Existing
    // states adopt the new Natural reading-order swipe default.
    std::vector<std::uint8_t> version6 = automatic_bytes;
    version6[4] = 6;
    version6[5] = 0;
    version6[39] &= 0x3FU;
    legacy_checksum = state_checksum(version6.data(), version6.size() - 4);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        version6[version6.size() - 4 + shift / 8] =
            static_cast<std::uint8_t>(legacy_checksum >> shift);
    }
    CHECK(nmarkdown::decode_reader_state(
        version6.data(), version6.size(), output, error));
    CHECK(output.natural_swiping);

    std::vector<std::uint8_t> version2 = version3;
    version2.erase(version2.begin() + 32);
    version2[4] = 2;
    version2[5] = 0;
    version2[6] = 40;
    version2[7] = 0;
    legacy_checksum = state_checksum(version2.data(), version2.size() - 4);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        version2[version2.size() - 4 + shift / 8] =
            static_cast<std::uint8_t>(legacy_checksum >> shift);
    }
    CHECK(nmarkdown::decode_reader_state(
        version2.data(), version2.size(), output, error));
    CHECK(output.line_gap == 7);
    CHECK(output.side_margin == 11);
    CHECK(output.reading_mode == nmarkdown::ReadingMode::VerticalScroll);
    CHECK(output.natural_scrolling);
    CHECK(output.natural_swiping);

    std::vector<std::uint8_t> legacy = version2;
    legacy.erase(legacy.begin() + 30, legacy.begin() + 32);
    legacy[4] = 1;
    legacy[5] = 0;
    legacy[6] = 38;
    legacy[7] = 0;
    legacy_checksum = state_checksum(legacy.data(), legacy.size() - 4);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        legacy[legacy.size() - 4 + shift / 8] =
            static_cast<std::uint8_t>(legacy_checksum >> shift);
    }
    CHECK(nmarkdown::decode_reader_state(
        legacy.data(), legacy.size(), output, error));
    CHECK(output.line_gap == 4);
    CHECK(output.side_margin == 5);
    CHECK(output.reading_mode == nmarkdown::ReadingMode::VerticalScroll);
    CHECK(output.natural_scrolling);
    CHECK(output.natural_swiping);

    bytes[12] ^= 0x80U;
    CHECK(!nmarkdown::decode_reader_state(bytes.data(), bytes.size(), output, error));
    CHECK(!nmarkdown::decode_reader_state(bytes.data(), 8, output, error));
}

}  // namespace

int main() {
    test_round_trip_and_corruption();
    if (failures != 0) {
        std::fprintf(stderr, "%d reader state test(s) failed\n", failures);
        return 1;
    }
    std::printf("All reader state tests passed\n");
    return 0;
}
