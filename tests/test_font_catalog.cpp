#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "nmarkdown/platform/platform.h"
#include "nmarkdown/text/font_catalog.h"

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(condition) check((condition), #condition, __LINE__)

std::string asset(const char* name) {
    return std::string(NMARKDOWN_SOURCE_DIR) + "/assets/fonts/" + name;
}

void test_real_font_metadata() {
    nmarkdown::StdioFileSystem files;
    std::string error;
    nmarkdown::FontFaceCatalogEntry regular;
    CHECK(nmarkdown::inspect_font_face(
        files, asset("DejaVuSans-CX.ttf"), regular, error));
    CHECK(regular.family == "DejaVu Sans");
    CHECK(!regular.italic);
    CHECK(!regular.fixed_pitch);
    CHECK(regular.has_latin);
    CHECK(!regular.has_cjk);
    CHECK(!regular.variable);

    nmarkdown::FontFaceCatalogEntry italic;
    CHECK(nmarkdown::inspect_font_face(
        files, asset("DejaVuSans-Oblique-CX.ttf"), italic, error));
    CHECK(italic.family == regular.family);
    CHECK(italic.italic);

    nmarkdown::FontFaceCatalogEntry mono;
    CHECK(nmarkdown::inspect_font_face(
        files, asset("DejaVuSansMono-CX.ttf"), mono, error));
    CHECK(mono.family == "DejaVu Sans Mono");
    CHECK(mono.fixed_pitch);
    CHECK(mono.has_latin);

    nmarkdown::FontFaceCatalogEntry cjk;
    CHECK(nmarkdown::inspect_font_face(
        files, asset("SarasaFixedSC-Regular-CX.ttf"), cjk, error));
    CHECK(cjk.family.find("Sarasa") != std::string::npos);
    CHECK(cjk.fixed_pitch);
    CHECK(!cjk.has_latin);
    CHECK(cjk.has_cjk);

}

}  // namespace

int main() {
    test_real_font_metadata();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "All font catalog tests passed\n";
    return EXIT_SUCCESS;
}
