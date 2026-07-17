#include <cstdio>
#include <string>
#include <vector>

#include "nmarkdown/document/unicode.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n",           \
                         __FILE__, __LINE__, #condition);                      \
            ++failures;                                                        \
        }                                                                      \
    } while (false)

void test_normalization() {
    CHECK(std::string(nmarkdown::unicode_data_version()) == "17.0.0");
    std::string output;
    const std::string composed = u8"Café Å Å 각";
    nmarkdown::Utf8ValidationResult result = nmarkdown::unicode_normalize_utf8(
        composed, nmarkdown::UnicodeNormalizationForm::Nfd, output);
    CHECK(result.valid());
    CHECK(output == std::string(u8"Café Å Å 각"));
    std::string recomposed;
    nmarkdown::unicode_normalize_utf8(
        output, nmarkdown::UnicodeNormalizationForm::Nfc, recomposed);
    CHECK(recomposed == std::string(u8"Café Å Å 각"));

    const std::string unordered = std::string("a") + u8"̀̕";
    nmarkdown::unicode_normalize_utf8(
        unordered, nmarkdown::UnicodeNormalizationForm::Nfd, output);
    CHECK(output == std::string("a") + u8"̀̕");
}

void test_case_folding_and_invalid_input() {
    std::vector<std::uint32_t> folded;
    nmarkdown::unicode_case_fold(0x00DF, folded);
    CHECK(folded.size() == 2 && folded[0] == 's' && folded[1] == 's');
    folded.clear();
    nmarkdown::unicode_case_fold(0x03A3, folded);
    CHECK(folded.size() == 1 && folded[0] == 0x03C3);

    std::string invalid("x\xFFy", 3);
    std::string output;
    const nmarkdown::Utf8ValidationResult result = nmarkdown::unicode_normalize_utf8(
        invalid, nmarkdown::UnicodeNormalizationForm::Nfc, output);
    CHECK(result.invalid_sequence_count == 1);
    CHECK(output == std::string(u8"x�y"));
}

}  // namespace

int main() {
    test_normalization();
    test_case_folding_and_invalid_input();
    if (failures != 0) {
        std::fprintf(stderr, "%d Unicode test(s) failed\n", failures);
        return 1;
    }
    std::printf("All Unicode normalization tests passed\n");
    return 0;
}
