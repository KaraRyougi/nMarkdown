#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "nmarkdown/document/utf8.h"

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

void check_decode(const std::vector<std::uint8_t>& bytes,
                  std::uint32_t value,
                  std::uint8_t length,
                  bool valid) {
    const nmarkdown::DecodedCodepoint decoded =
        nmarkdown::utf8_next(bytes.data(), bytes.size(), 0);
    CHECK(decoded.value == value);
    CHECK(decoded.byte_length == length);
    CHECK(decoded.valid == valid);
}

void test_valid_boundaries() {
    check_decode({0x00}, 0x0000U, 1, true);
    check_decode({0x7F}, 0x007FU, 1, true);
    check_decode({0xC2, 0x80}, 0x0080U, 2, true);
    check_decode({0xDF, 0xBF}, 0x07FFU, 2, true);
    check_decode({0xE0, 0xA0, 0x80}, 0x0800U, 3, true);
    check_decode({0xEF, 0xBF, 0xBF}, 0xFFFFU, 3, true);
    check_decode({0xF0, 0x90, 0x80, 0x80}, 0x10000U, 4, true);
    check_decode({0xF4, 0x8F, 0xBF, 0xBF}, 0x10FFFFU, 4, true);
}

void test_invalid_sequences() {
    check_decode({0x80}, nmarkdown::kReplacementCodepoint, 1, false);
    check_decode({0xC0, 0xAF}, nmarkdown::kReplacementCodepoint, 1, false);
    check_decode({0xE0, 0x80, 0x80}, nmarkdown::kReplacementCodepoint, 3, false);
    check_decode({0xED, 0xA0, 0x80}, nmarkdown::kReplacementCodepoint, 3, false);
    check_decode({0xF4, 0x90, 0x80, 0x80}, nmarkdown::kReplacementCodepoint, 4, false);
    check_decode({0xF5, 0x80, 0x80, 0x80}, nmarkdown::kReplacementCodepoint, 1, false);
    check_decode({0xE2, 0x82}, nmarkdown::kReplacementCodepoint, 2, false);
    check_decode({0xE2, 0x28, 0xA1}, nmarkdown::kReplacementCodepoint, 1, false);
}

void test_sanitization_and_progress() {
    const std::uint8_t input[] = {
        0xEF, 0xBB, 0xBF, 'A', 0xE2, 0x28, 0xA1, 0x00, 0xCE, 0xA9,
    };
    std::string output;
    const nmarkdown::Utf8ValidationResult result =
        nmarkdown::utf8_sanitize(input, sizeof(input), output);
    CHECK(result.had_bom);
    CHECK(result.codepoint_count == 6);
    CHECK(result.invalid_sequence_count == 2);

    const std::string expected = std::string("A") + "\xEF\xBF\xBD(" +
                                 "\xEF\xBF\xBD" + std::string(1, '\0') +
                                 "\xCE\xA9";
    CHECK(output == expected);

    const nmarkdown::Utf8ValidationResult valid = nmarkdown::utf8_validate(
        reinterpret_cast<const std::uint8_t*>(output.data()), output.size());
    CHECK(valid.valid());
    CHECK(valid.codepoint_count == 6);
}

void test_append_rejects_non_scalars() {
    std::string output;
    nmarkdown::utf8_append(0xD800U, output);
    nmarkdown::utf8_append(0x110000U, output);
    CHECK(output == "\xEF\xBF\xBD\xEF\xBF\xBD");
}

}  // namespace

int main() {
    test_valid_boundaries();
    test_invalid_sequences();
    test_sanitization_and_progress();
    test_append_rejects_non_scalars();

    if (failures != 0) {
        std::fprintf(stderr, "%d UTF-8 test(s) failed\n", failures);
        return 1;
    }
    std::printf("All UTF-8 tests passed\n");
    return 0;
}

