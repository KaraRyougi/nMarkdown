#include <cstdio>
#include <string>

#include "nmarkdown/document/search.h"

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

void test_exact_and_ascii_fold() {
    const std::string source = u8"Alpha αβγ\nalpha ALPHA café Café αβγ";
    auto matches = nmarkdown::search_source(
        source, "Alpha", nmarkdown::SearchMode::ExactUtf8);
    CHECK(matches.size() == 1);
    CHECK(matches[0].source_offset == 0);
    matches = nmarkdown::search_source(
        source, "alpha", nmarkdown::SearchMode::AsciiCaseInsensitive);
    CHECK(matches.size() == 3);
    matches = nmarkdown::search_source(
        source, u8"αβγ", nmarkdown::SearchMode::AsciiCaseInsensitive);
    CHECK(matches.size() == 2);
    matches = nmarkdown::search_source(
        source, u8"café", nmarkdown::SearchMode::AsciiCaseInsensitive);
    CHECK(matches.size() == 2);
}

void test_utf8_safe_snippet_and_limit() {
    std::string source;
    for (int index = 0; index < 100; ++index) source += u8"λ needle Ω\n";
    const auto matches = nmarkdown::search_source(
        source, "needle", nmarkdown::SearchMode::ExactUtf8, 5);
    CHECK(matches.size() == 5);
    for (const nmarkdown::SearchMatch& match : matches) {
        CHECK(match.snippet.find("needle") != std::string::npos);
        CHECK(match.snippet.find('\n') == std::string::npos);
        CHECK(match.snippet_match_offset < match.snippet.size());
        if (!match.snippet.empty()) {
            CHECK((static_cast<unsigned char>(match.snippet[0]) & 0xC0U) != 0x80U);
        }
    }
}

void test_unicode_fold_and_canonical_equivalence() {
    const std::string source = u8"Straße STRASSE Café Café Σ σ";
    auto matches = nmarkdown::search_source(
        source, "strasse", nmarkdown::SearchMode::UnicodeCaseInsensitive);
    CHECK(matches.size() == 2);
    CHECK(matches.size() < 1 || matches[0].source_length == std::string(u8"Straße").size());
    matches = nmarkdown::search_source(
        source, "strasse", nmarkdown::SearchMode::AsciiCaseInsensitive);
    CHECK(matches.size() == 1);

    matches = nmarkdown::search_source(
        source, u8"Café", nmarkdown::SearchMode::CanonicalUtf8);
    CHECK(matches.size() == 2);
    CHECK(matches.size() < 2 || matches[0].source_length != matches[1].source_length);
    matches = nmarkdown::search_source(
        source, u8"σ", nmarkdown::SearchMode::UnicodeCaseInsensitive);
    CHECK(matches.size() == 2);

    const std::string reordered = std::string("a") + u8"̀̕";
    matches = nmarkdown::search_source(
        reordered, std::string("a") + u8"̀̕",
        nmarkdown::SearchMode::CanonicalUtf8);
    CHECK(matches.size() == 1);
    CHECK(matches.size() < 1 || matches[0].source_length == reordered.size());
}

void test_snippets_are_reader_facing() {
    const std::string source =
        "# Comparison heading\n\n"
        "A deliberately long prefix before the [comparison link](chapter.md) "
        "and enough trailing words to require omitted context in the result.\n\n"
        "| comparison | value |\n| --- | --- |\n";
    const auto matches = nmarkdown::search_source(
        source, "comparison", nmarkdown::SearchMode::AsciiCaseInsensitive);
    CHECK(matches.size() == 3);
    for (const nmarkdown::SearchMatch& match : matches) {
        CHECK(match.snippet.find('#') == std::string::npos);
        CHECK(match.snippet.find('|') == std::string::npos);
        CHECK(match.snippet.find("[comparison]") == std::string::npos);
        CHECK(match.snippet_match_offset < match.snippet.size());
    }
    CHECK(matches.size() < 2 ||
          matches[1].snippet.find(u8"…") != std::string::npos);
}

}  // namespace

int main() {
    test_exact_and_ascii_fold();
    test_utf8_safe_snippet_and_limit();
    test_unicode_fold_and_canonical_equivalence();
    test_snippets_are_reader_facing();
    if (failures != 0) {
        std::fprintf(stderr, "%d search test(s) failed\n", failures);
        return 1;
    }
    std::printf("All search tests passed\n");
    return 0;
}
