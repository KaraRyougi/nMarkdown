#include <array>
#include <cstdint>
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

std::uint64_t hash_pixels(const std::vector<std::uint16_t>& pixels) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::uint16_t pixel : pixels) {
        hash ^= static_cast<std::uint8_t>(pixel);
        hash *= 1099511628211ULL;
        hash ^= static_cast<std::uint8_t>(pixel >> 8U);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void test_formula_corpus_golden() {
    const std::array<const char*, 15> corpus{{
        R"(\alpha+\beta=\Gamma,\quad x\in A\subseteq B)",
        R"(\pm\mp\times\div\cdot\quad a\le b\ne c\approx d)",
        R"(\frac{-b\pm\sqrt{b^2-4ac}}{2a})",
        R"(\frac{1+\frac{x}{y}}{1-\frac{x}{y}}+\sqrt[3]{\frac{x^2+1}{y_0}})",
        R"(x_i^2+x_{i_j}^{n+1}+e^{-x^2}+a^{b^{c^d}})",
        R"(\sum_{i=0}^{n}i^2+\prod_{k=1}^{n}k+\int_a^b f(x))",
        R"(\left\{\frac{x^2}{y_1}\right\}\quad\langle u,v\rangle)",
        R"(\left(\frac{x+1}{x-1}\right)\quad
            \left|\frac{x^2}{y_1}\right|)",
        R"(\hat{x}+\bar{y}+\vec{v}+\ddot{q}+\underline{z})",
        R"(\mathrm{sin}(x)+\mathit{velocity}+\mathbf{F}=m\mathbf{a})",
        R"(\mathbb{R}^2\to\mathbb{R}\quad\mathcal{F}(x))",
        R"(\begin{pmatrix}a&b\\c&d\end{pmatrix}\mathbf{x}
            =\begin{bmatrix}1\\0\end{bmatrix})",
        R"(\begin{bmatrix}1&0&0\\0&1&0\\0&0&1\end{bmatrix})",
        R"(\begin{cases}x+y=3\\2x-y=0\end{cases})",
        R"(\begin{aligned}f(x)&=x^2+2x+1\\&=(x+1)^2\end{aligned})",
    }};

    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    std::vector<std::uint16_t> pixels(320 * 1200, 0xFFFFU);
    nmarkdown::Surface565 surface(pixels.data(), 320, 1200, 320);
    int top = 8;
    for (const char* formula : corpus) {
        std::shared_ptr<const nmarkdown::MathLayoutResult> layout;
        CHECK(math.layout(formula, nmarkdown::MathStyle::Display,
                          nmarkdown::fx_from_int(17),
                          nmarkdown::fx_from_int(310), layout));
        CHECK(layout != nullptr);
        if (layout == nullptr) continue;
        CHECK(layout->valid);
        CHECK(layout->diagnostic.empty());
        const int baseline = top + nmarkdown::fx_ceil(layout->metrics.ascent);
        CHECK(math.draw(surface, *layout, 5, baseline, 0, 0x2948U, 0xFFFFU,
                        false, {0, 0, 320, 1200}));
        top = baseline + nmarkdown::fx_ceil(layout->metrics.descent) + 12;
    }
    CHECK(top < 1200);
    const std::uint64_t actual = hash_pixels(pixels);
    // Re-pinned for the balanced default in the 0 = extra-smooth,
    // 10 = former Sharpness 7 coverage range.
    constexpr std::uint64_t kExpected = 0x2CB4E5402ED0353CULL;
    if (actual != kExpected) {
        std::fprintf(stderr, "math golden hash: 0x%016llx\n",
                     static_cast<unsigned long long>(actual));
    }
    CHECK(actual == kExpected);
}

void test_extended_formula_corpus_golden() {
    const std::array<const char*, 9> corpus{{
        R"(\zeta+\eta+\iota+\kappa+\nu+\xi+\omicron+\tau+\upsilon+\varphi+\chi)",
        R"(\ngeq+\ast+\because+\therefore+\backsim+\cong+\forall+\exists+\angle)",
        R"(\iint+\iiint+\oint+\bigodot+\bigoplus+\bigotimes)",
        R"(\overleftarrow{a+b+c}+\overrightarrow{x+y}+\underleftrightarrow{u+v})",
        R"(\overbrace{a+\underbrace{b+c}_{1.0}}^{2.0})",
        R"(\rm D+\cal D+\it D+\Bbb D+\bf D+\sf D+\tt D+\frak D)",
        R"(i+j+\imath+\jmath+\hat{\imath}+\vec{\jmath})",
        R"(\begin{Bmatrix}1&2\\3&4\end{Bmatrix}+\begin{vmatrix}a&b\\c&d\end{vmatrix}+\begin{Vmatrix}x&y\\z&w\end{Vmatrix})",
        R"(\begin{array}{c|lr}n&a&b\\\hline1&2&3\\4&5&6\end{array}+\cfrac{1}{1+\cfrac{1}{x}})",
    }};

    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    std::vector<std::uint16_t> pixels(320 * 900, 0xFFFFU);
    nmarkdown::Surface565 surface(pixels.data(), 320, 900, 320);
    int top = 8;
    for (const char* formula : corpus) {
        std::shared_ptr<const nmarkdown::MathLayoutResult> layout;
        CHECK(math.layout(formula, nmarkdown::MathStyle::Display,
                          nmarkdown::fx_from_int(16),
                          nmarkdown::fx_from_int(310), layout));
        CHECK(layout != nullptr);
        if (layout == nullptr) continue;
        CHECK(layout->valid);
        CHECK(layout->diagnostic.empty());
        const int baseline = top + nmarkdown::fx_ceil(layout->metrics.ascent);
        CHECK(math.draw(surface, *layout, 5, baseline, 0, 0x2948U, 0xFFFFU,
                        false, {0, 0, 320, 900}));
        top = baseline + nmarkdown::fx_ceil(layout->metrics.descent) + 12;
    }
    CHECK(top < 900);
    const std::uint64_t actual = hash_pixels(pixels);
    constexpr std::uint64_t kExpected = 0xF259D905F4EBA244ULL;
    if (actual != kExpected) {
        std::fprintf(stderr, "extended math golden hash: 0x%016llx\n",
                     static_cast<unsigned long long>(actual));
    }
    CHECK(actual == kExpected);
}

void test_native_symbol_gallery_golden() {
    const std::array<const char*, 9> corpus{{
        R"(\dot x+\cup+\N+\lang x\rang+\empty+p\or q+x\rightarrow y)",
        R"(A\textvisiblespace B+\text{A\textvisiblespace B}+A\space B)",
        R"(\epsilon+\varepsilon+\vartheta+\varpi+\varrho+\varsigma+\ell+\hbar+\aleph)",
        R"(A\wedge B\vee C\oplus D\otimes E\quad x\nleq y\parallel z\perp w\preceq q)",
        R"(x\mapsto y\hookrightarrow z\rightleftharpoons w\quad\nRightarrow\quad\longleftrightarrow)",
        R"(\coprod_{i=0}^{n}+\bigcup_{j=1}^{m}+\bigsqcup A_i+\iiiint_V f\,dV+\oiint_S F)",
        R"(\left\|\frac{x^2+1}{y}\right\|+\left\llbracket z\right\rrbracket+\not=+\not\in)",
        R"(\dot{x}+\ddot y+\acute z+\grave q+\breve a+\check b+\tilde c+\mathring d+\vec v)",
        R"(\mathsf{Sans}+\mathtt{Mono}+\mathfrak{Fraktur}+\mathscr{F}+\mathcal{lowercase})",
    }};

    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));
    nmarkdown::MathSystem math(text);
    std::vector<std::uint16_t> pixels(320 * 900, 0xFFFFU);
    nmarkdown::Surface565 surface(pixels.data(), 320, 900, 320);
    int top = 8;
    for (const char* formula : corpus) {
        std::shared_ptr<const nmarkdown::MathLayoutResult> layout;
        CHECK(math.layout(formula, nmarkdown::MathStyle::Display,
                          nmarkdown::fx_from_int(16),
                          nmarkdown::fx_from_int(310), layout));
        CHECK(layout != nullptr);
        if (layout == nullptr) continue;
        CHECK(layout->valid);
        CHECK(layout->diagnostic.empty());
        const int baseline = top + nmarkdown::fx_ceil(layout->metrics.ascent);
        CHECK(math.draw(surface, *layout, 5, baseline, 0, 0x2948U, 0xFFFFU,
                        false, {0, 0, 320, 900}));
        top = baseline + nmarkdown::fx_ceil(layout->metrics.descent) + 12;
    }
    CHECK(top < 900);
    const std::uint64_t actual = hash_pixels(pixels);
    constexpr std::uint64_t kExpected = 0x4F11D1E9B7DFB0ABULL;
    if (actual != kExpected) {
        std::fprintf(stderr, "native symbol gallery hash: 0x%016llx\n",
                     static_cast<unsigned long long>(actual));
    }
    CHECK(actual == kExpected);
}

}  // namespace

int main() {
    test_formula_corpus_golden();
    test_extended_formula_corpus_golden();
    test_native_symbol_gallery_golden();
    if (failures != 0) {
        std::fprintf(stderr, "%d math golden test(s) failed\n", failures);
        return 1;
    }
    std::printf("All math golden tests passed\n");
    return 0;
}
