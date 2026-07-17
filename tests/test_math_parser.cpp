#include <cstdio>
#include <array>
#include <string>
#include <string_view>

#include "nmarkdown/math/math_lexer.h"
#include "nmarkdown/math/math_macros.h"
#include "nmarkdown/math/math_parser.h"

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

std::size_t count_kind(const nmarkdown::MathTree& tree,
                       nmarkdown::MathNodeKind kind) {
    std::size_t count = 0;
    for (const nmarkdown::MathNode& node : tree.nodes) {
        if (node.kind == kind) ++count;
    }
    return count;
}

void test_lexer() {
    nmarkdown::MathLexResult result;
    CHECK(nmarkdown::lex_math(u8"\\frac{α_1}{x^2} \\; y", result));
    CHECK(result.ok());
    CHECK(result.tokens.back().kind == nmarkdown::MathTokenKind::End);
    CHECK(result.tokens[0].kind == nmarkdown::MathTokenKind::ControlSequence);
    CHECK(nmarkdown::math_token_text(u8"\\frac{α_1}{x^2} \\; y", result.tokens[0]) ==
          "frac");
}

void test_structures_and_symbols() {
    const std::string source =
        u8"\\left(\\frac{α_i^2+\\sqrt[n]{x}}{\\sum_{i=0}^{n}}\\right)";
    nmarkdown::MathTree tree;
    CHECK(nmarkdown::parse_math(source, tree));
    CHECK(!tree.recovered_error);
    CHECK(tree.root != nmarkdown::kInvalidMathNode);
    CHECK(count_kind(tree, nmarkdown::MathNodeKind::Delimited) == 1);
    CHECK(count_kind(tree, nmarkdown::MathNodeKind::Fraction) == 1);
    CHECK(count_kind(tree, nmarkdown::MathNodeKind::Radical) == 1);
    CHECK(count_kind(tree, nmarkdown::MathNodeKind::Scripts) >= 2);
    for (const nmarkdown::MathNode& node : tree.nodes) {
        if (node.kind == nmarkdown::MathNodeKind::Radical) CHECK(node.child_count == 2);
    }
    CHECK(nmarkdown::parse_math("\\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}", tree));
    CHECK(!tree.recovered_error);
    CHECK(nmarkdown::parse_math("\\frac{x}\n{y}", tree));
    CHECK(!tree.recovered_error);
    CHECK(nmarkdown::parse_math("\\left.\\frac{du}{dx}\\right| _{x=0}", tree));
    CHECK(!tree.recovered_error);

    CHECK(nmarkdown::parse_math("\\imath+\\jmath", tree));
    CHECK(!tree.recovered_error);
    bool found_imath = false;
    bool found_jmath = false;
    for (const nmarkdown::MathNode& node : tree.nodes) {
        if (node.kind != nmarkdown::MathNodeKind::Symbol) continue;
        if (tree.text(node) == u8"𝒊") {
            found_imath = true;
        }
        if (tree.text(node) == u8"𝒋") {
            found_jmath = true;
        }
    }
    CHECK(found_imath);
    CHECK(found_jmath);
}

void test_accents_styles_and_matrix() {
    const std::string source =
        "\\mathbf{x}+\\hat{y}+\\begin{pmatrix}a&b\\\\c&d\\end{pmatrix}";
    nmarkdown::MathTree tree;
    CHECK(nmarkdown::parse_math(source, tree));
    CHECK(!tree.recovered_error);
    CHECK(count_kind(tree, nmarkdown::MathNodeKind::Styled) == 1);
    CHECK(count_kind(tree, nmarkdown::MathNodeKind::Accent) == 1);
    CHECK(count_kind(tree, nmarkdown::MathNodeKind::Array) == 1);
    for (const nmarkdown::MathNode& node : tree.nodes) {
        if (node.kind == nmarkdown::MathNodeKind::Array) {
            CHECK(node.aux == 2);
            CHECK(node.value == 2);
        }
    }
    CHECK(nmarkdown::parse_math("a&=b+c\\\\d&=e-f", tree));
    CHECK(!tree.recovered_error);
    CHECK(tree.root < tree.nodes.size());
    if (tree.root < tree.nodes.size()) {
        CHECK(tree.nodes[tree.root].kind == nmarkdown::MathNodeKind::Array);
        CHECK(tree.nodes[tree.root].aux == 2);
        CHECK(tree.nodes[tree.root].value == 2);
    }
}

void test_complete_native_symbol_catalog() {
    std::string_view previous;
    for (const ExpectedSymbol& expected : kExpectedSymbols) {
        const std::string_view name(expected.name);
        CHECK(previous.empty() || previous < name);
        previous = name;

        nmarkdown::MathTree tree;
        const std::string source = "\\" + std::string(name);
        const bool parsed = nmarkdown::parse_math(source, tree);
        if (!parsed || tree.recovered_error) {
            std::fprintf(stderr, "symbol catalog command \\%s: %s at %u\n",
                         expected.name, tree.diagnostic.c_str(),
                         tree.diagnostic_offset);
            ++failures;
            continue;
        }

        const nmarkdown::MathNode* symbol = nullptr;
        for (const nmarkdown::MathNode& node : tree.nodes) {
            if (node.kind == nmarkdown::MathNodeKind::Symbol &&
                tree.text(node) == expected.text) {
                symbol = &node;
                break;
            }
        }
        if (symbol == nullptr) {
            std::fprintf(stderr, "symbol catalog command \\%s produced no %s\n",
                         expected.name, expected.text);
            ++failures;
            continue;
        }
        CHECK(symbol->atom_class == expected.atom_class);
        CHECK(((symbol->flags & nmarkdown::MathNodeFlagLargeOperator) != 0) ==
              expected.large_glyph);
        CHECK(((symbol->flags & nmarkdown::MathNodeFlagMovableLimits) != 0) ==
              expected.movable_limits);
    }

    // These expectations are intentionally independent of the generated
    // catalog above. They pin the semantic distinctions that control spacing,
    // delimiters, display variants, and script placement.
    struct SemanticAnchor {
        const char* command;
        const char* text;
        nmarkdown::AtomClass atom_class;
        bool large_glyph;
        bool movable_limits;
    };
    const std::array<SemanticAnchor, 13> semantic_anchors{{
        {R"(\cup)", u8"∪", nmarkdown::AtomClass::Binary, false, false},
        {R"(\rightarrow)", u8"→", nmarkdown::AtomClass::Relation, false, false},
        {R"(\lang)", u8"⟨", nmarkdown::AtomClass::Opening, false, false},
        {R"(\rang)", u8"⟩", nmarkdown::AtomClass::Closing, false, false},
        {R"(\sum)", u8"∑", nmarkdown::AtomClass::Operator, true, true},
        {R"(\int)", u8"∫", nmarkdown::AtomClass::Operator, true, false},
        {R"(\intop)", u8"∫", nmarkdown::AtomClass::Operator, true, true},
        {R"(\smallint)", u8"∫", nmarkdown::AtomClass::Operator, false, true},
        {R"(\lim)", "lim", nmarkdown::AtomClass::Operator, false, true},
        {R"(\sin)", "sin", nmarkdown::AtomClass::Operator, false, false},
        {R"(\empty)", u8"∅", nmarkdown::AtomClass::Ordinary, false, false},
        {R"(\cdots)", u8"⋯", nmarkdown::AtomClass::Inner, false, false},
        {R"(\backslash)", "\\", nmarkdown::AtomClass::Ordinary, false, false},
    }};
    for (const SemanticAnchor& expected : semantic_anchors) {
        nmarkdown::MathTree anchor_tree;
        CHECK(nmarkdown::parse_math(expected.command, anchor_tree));
        CHECK(!anchor_tree.recovered_error);
        const nmarkdown::MathNode* anchor = nullptr;
        for (const nmarkdown::MathNode& node : anchor_tree.nodes) {
            if (node.kind == nmarkdown::MathNodeKind::Symbol) {
                anchor = &node;
                break;
            }
        }
        CHECK(anchor != nullptr);
        if (anchor == nullptr) continue;
        CHECK(anchor_tree.text(*anchor) == expected.text);
        CHECK(anchor->atom_class == expected.atom_class);
        CHECK(((anchor->flags & nmarkdown::MathNodeFlagLargeOperator) != 0) ==
              expected.large_glyph);
        CHECK(((anchor->flags & nmarkdown::MathNodeFlagMovableLimits) != 0) ==
              expected.movable_limits);
    }

    nmarkdown::MathTree tree;
    CHECK(nmarkdown::parse_math(
        R"(\dot x+\cup+\N+\lang x\rang+\empty+p\or q+x\rightarrow y)",
        tree));
    CHECK(!tree.recovered_error);
    CHECK(count_kind(tree, nmarkdown::MathNodeKind::Accent) == 1);

    CHECK(nmarkdown::parse_math(R"(\not=+\not\in+\not{\subset})", tree));
    CHECK(!tree.recovered_error);
    std::size_t composed_negations = 0;
    for (const nmarkdown::MathNode& node : tree.nodes) {
        if (node.kind == nmarkdown::MathNodeKind::Symbol &&
            tree.text(node).find(u8"\u0338") != std::string_view::npos) {
            ++composed_negations;
        }
    }
    CHECK(composed_negations == 3);

    CHECK(nmarkdown::parse_math(R"(\not\sin x)", tree));
    CHECK(tree.recovered_error);
    CHECK(nmarkdown::parse_math(R"(\not{a=b})", tree));
    CHECK(tree.recovered_error);

    CHECK(nmarkdown::parse_math(
        R"(\left\lang x\right\rang+\left\|y\right\|+\Vert z\Vert)",
        tree));
    CHECK(!tree.recovered_error);
    bool found_angle_delimiters = false;
    bool found_double_bar_delimiters = false;
    for (const nmarkdown::MathNode& node : tree.nodes) {
        if (node.kind != nmarkdown::MathNodeKind::Delimited) continue;
        const std::string_view descriptor = tree.text(node);
        found_angle_delimiters = found_angle_delimiters ||
            descriptor == std::string_view(u8"⟨\0⟩", 7);
        found_double_bar_delimiters = found_double_bar_delimiters ||
            descriptor == std::string_view(u8"‖\0‖", 7);
    }
    CHECK(found_angle_delimiters);
    CHECK(found_double_bar_delimiters);

    CHECK(nmarkdown::parse_math(R"(\left\sum x\right\cup)", tree));
    CHECK(tree.recovered_error);

    const std::array<std::pair<const char*, std::uint32_t>, 3>
        invalid_delimiters{{
        {R"(\left a x\right b)", 6},
        {R"(\left+ x\right-)", 5},
        {R"(\left\%x\right\?)", 6},
    }};
    for (const auto& invalid : invalid_delimiters) {
        CHECK(nmarkdown::parse_math(invalid.first, tree));
        CHECK(tree.recovered_error);
        CHECK(tree.diagnostic_offset == invalid.second);
    }

    CHECK(nmarkdown::parse_math(
        R"(\left(x\right)+\left[x\right]+\left\{x\right\}+)"
        R"(\left\uparrow x\right\Downarrow)",
        tree));
    CHECK(!tree.recovered_error);

    CHECK(nmarkdown::parse_math(
        R"(\mathnormal{x}+\mathsf{x}+\mathtt{x}+\mathfrak{x}+\mathscr{F}+\bm{x})",
        tree));
    CHECK(!tree.recovered_error);

    CHECK(nmarkdown::parse_math(R"(\bm{\sum}_0^1)", tree));
    CHECK(!tree.recovered_error);
    bool styled_movable_sum = false;
    for (const nmarkdown::MathNode& node : tree.nodes) {
        styled_movable_sum = styled_movable_sum ||
            (node.kind == nmarkdown::MathNodeKind::Styled &&
             node.atom_class == nmarkdown::AtomClass::Operator &&
             (node.flags & nmarkdown::MathNodeFlagLargeOperator) != 0 &&
             (node.flags & nmarkdown::MathNodeFlagMovableLimits) != 0);
    }
    CHECK(styled_movable_sum);
}

void test_recovery_and_limits() {
    nmarkdown::MathTree tree;
    CHECK(nmarkdown::parse_math("\\frac{x}{", tree));
    CHECK(tree.recovered_error);
    CHECK(!tree.diagnostic.empty());
    CHECK(nmarkdown::parse_math("x+\\unknown{y}", tree));
    CHECK(tree.recovered_error);
    std::string huge(nmarkdown::kMaximumFormulaBytes + 1, 'x');
    CHECK(!nmarkdown::parse_math(huge, tree));
}

void test_visible_space_text_command() {
    nmarkdown::MathTree tree;
    CHECK(nmarkdown::parse_math(R"(\textvisiblespace)", tree));
    CHECK(!tree.recovered_error);
    bool found_direct = false;
    for (const nmarkdown::MathNode& node : tree.nodes) {
        if (node.kind == nmarkdown::MathNodeKind::Symbol &&
            tree.text(node) == u8"␣") {
            found_direct = true;
            CHECK(node.atom_class == nmarkdown::AtomClass::Ordinary);
            CHECK(node.flags == nmarkdown::MathNodeFlagNone);
        }
    }
    CHECK(found_direct);

    const std::array<std::pair<const char*, const char*>, 3> wrapped{{
        {R"(\text{A\textvisiblespace B})", u8"A␣B"},
        {R"(\text{C\textvisiblespace{}D})", u8"C␣D"},
        {R"(\text{\\textvisiblespace + \unknown})",
         R"(\\textvisiblespace + \unknown)"},
    }};
    for (const auto& expected : wrapped) {
        CHECK(nmarkdown::parse_math(expected.first, tree));
        CHECK(!tree.recovered_error);
        bool found_text = false;
        for (const nmarkdown::MathNode& node : tree.nodes) {
            found_text = found_text ||
                (node.kind == nmarkdown::MathNodeKind::Text &&
                 tree.text(node) == expected.second);
        }
        CHECK(found_text);
    }

    // A visible-space marker is a glyph, unlike \space's invisible width.
    CHECK(nmarkdown::parse_math(R"(\space)", tree));
    CHECK(!tree.recovered_error);
    CHECK(count_kind(tree, nmarkdown::MathNodeKind::Space) == 1);
    for (const nmarkdown::MathNode& node : tree.nodes) {
        CHECK(tree.text(node) != u8"␣");
    }
}

void test_safe_macro_aliases() {
    nmarkdown::MathMacroExpansion expanded;
    CHECK(nmarkdown::expand_safe_math_macros(
        "\\newcommand{\\RR}{\\mathbb{R}} \\RR^2", expanded));
    CHECK(expanded.definition_count == 1);
    CHECK(expanded.source.find("\\mathbb{R}") != std::string::npos);
    nmarkdown::MathTree tree;
    CHECK(nmarkdown::parse_math(
        "\\newcommand{\\RR}{\\mathbb{R}} \\RR^2", tree));
    CHECK(!tree.recovered_error);

    CHECK(!nmarkdown::expand_safe_math_macros(
        "\\newcommand{\\a}{\\b}\\newcommand{\\b}{\\a} \\a", expanded));
    CHECK(expanded.error.find("recursive") != std::string::npos);
    CHECK(!nmarkdown::expand_safe_math_macros(
        "\\newcommand{\\f}{#1} \\f", expanded));
    CHECK(expanded.error.find("parameters") != std::string::npos);
    CHECK(!nmarkdown::expand_safe_math_macros(
        "\\newcommand{\\frac}{x} x", expanded));
    CHECK(expanded.error.find("reserved") != std::string::npos);
}

void test_markdown_formula_document_coverage() {
    // Every command/environment family used by markdown-formula.md.tns is
    // represented here. Keep these grouped so a parser diagnostic identifies
    // the unsupported family instead of replacing an entire long formula.
    const std::array<const char*, 18> formulas{{
        R"(\alpha+\beta+\gamma+\delta+\epsilon+\zeta+\eta+\theta+\iota+\kappa+\lambda+\mu+\nu+\xi+\omicron+\pi+\rho+\sigma+\tau+\upsilon+\phi+\varphi+\chi+\psi+\omega)",
        R"(\Gamma+\Delta+\Theta+\Lambda+\Xi+\Pi+\Sigma+\Upsilon+\Phi+\Psi+\Omega)",
        R"(\pm\mp\times\div\cdot\ast\cup\cap\setminus\bigodot\bigoplus\bigotimes)",
        R"(\neq\geq\leq\approx\ngeq\in\notin\subset\subseteq\supset\supseteq\backsim\cong\mid)",
        R"(\rightarrow\leftarrow\Rightarrow\Leftarrow\Uparrow\Downarrow\uparrow\downarrow\longrightarrow\longleftarrow\Longrightarrow\Longleftarrow\implies\impliedby\iff)",
        R"(\infty+\partial+\nabla+\imath+\jmath+\forall+\exists+\because+\therefore+\angle+\prime+\ldots+\cdots+\vdots+\ddots+\backslash)",
        R"(\sum_0^3+\prod_{i=0}^n+\int_0^1+\iint+\iiint+\oint+\lim_{x\to0}+\sin x+\cos x+\tan x+\cot x+\log x+\lg x+\ln x)",
        R"(\left\lfloor\frac{a}{b}\right\rfloor+\left\lceil\frac{c}{d}\right\rceil+\lbrace x\rbrace+\left\langle x\right\rangle+\left/ x\right\backslash)",
        R"(\hat{x}+\bar{x}+\vec{x}+\dot{x}+\ddot{x}+\overline{a+b}+\underline{a+b}+\overleftarrow{a+b}+\overrightarrow{a+b}+\overleftrightarrow{a+b}+\underleftarrow{a+b}+\underrightarrow{a+b}+\underleftrightarrow{a+b}+\overbrace{a+b}^{n}+\underbrace{a+b}_{n})",
        R"(\rm D+\cal D+\it D+\Bbb D+\bf D+\mit D+\sf D+\scr D+\tt D+\frak D+\boldsymbol D+\mathrm{D}+\mathbf{D}+\mathit{D}+\mathbb{D}+\mathcal{D})",
        R"(\displaystyle x=\cfrac{1}{1+\cfrac{1}{x}}\tag{公式1})",
        R"(f(n)=\begin{cases}n/2&\text{if $n$ is even}\\3n+1&\text{if $n$ is odd}\end{cases})",
        R"(\begin{matrix}1&2\\3&4\end{matrix}+\begin{pmatrix}1&2\\3&4\end{pmatrix}+\begin{bmatrix}1&2\\3&4\end{bmatrix})",
        R"(\begin{Bmatrix}1&2\\3&4\end{Bmatrix}+\begin{vmatrix}1&2\\3&4\end{vmatrix}+\begin{Vmatrix}1&2\\3&4\end{Vmatrix})",
        R"(\begin{array}{c|lcr}n&\text{左对齐}&\text{居中对齐}&\text{右对齐}\\\hline1&0.24&1&125\end{array})",
        R"(\left\{\begin{array}{c}a_1x+b_1y=d_1\\a_2x+b_2y=d_2\end{array}\right.)",
        R"(\begin{aligned}f(x)&=x^2+2x+1\\&=(x+1)^2\end{aligned})",
        R"(\begin{align}v+w&=0&\text{Given}\tag 1\\-w&=-w+0&\text{identity}\tag 2\end{align})",
    }};

    for (std::size_t index = 0; index < formulas.size(); ++index) {
        nmarkdown::MathTree tree;
        const bool parsed = nmarkdown::parse_math(formulas[index], tree);
        if (!parsed || tree.recovered_error) {
            std::fprintf(stderr, "formula coverage case %zu: %s at %u\n",
                         index + 1, tree.diagnostic.c_str(),
                         tree.diagnostic_offset);
            ++failures;
        }
    }

    nmarkdown::MathTree array;
    CHECK(nmarkdown::parse_math(
        R"(\begin{array}{c|l}a&b\\\hline c&d\end{array})", array));
    CHECK(!array.recovered_error);
    const nmarkdown::MathNode* array_node = nullptr;
    for (const nmarkdown::MathNode& node : array.nodes) {
        if (node.kind == nmarkdown::MathNodeKind::Array) array_node = &node;
    }
    CHECK(array_node != nullptr);
    if (array_node != nullptr) {
        CHECK((array_node->metadata & (1U << 1U)) != 0);
        CHECK(array.text(*array_node).find("c|l") != std::string_view::npos);
    }
}

void test_large_align_formula_structure() {
    // Regression fixture for a display formula that is intentionally wider
    // than the calculator viewport.  Parsing must preserve the three aligned
    // rows and their explanatory text so the viewer can offer horizontal pan.
    constexpr const char* source = R"(\begin{align}
    v + w & = 0  & \text{Given} \tag 1 \\
       -w & = -w + 0 & \text{additive identity} \tag 2 \\
   -w + 0 & = -w + (v + w) & \text{equations $(1)$ and $(2)$} \\
\end{align})";

    nmarkdown::MathTree tree;
    CHECK(nmarkdown::parse_math(source, tree));
    CHECK(!tree.recovered_error);
    CHECK(tree.root != nmarkdown::kInvalidMathNode);
    if (tree.root == nmarkdown::kInvalidMathNode ||
        tree.root >= tree.nodes.size()) {
        return;
    }

    const nmarkdown::MathNode* aligned = nullptr;
    for (const nmarkdown::MathNode& node : tree.nodes) {
        if (node.kind == nmarkdown::MathNodeKind::Array &&
            tree.text(node) == "align") {
            aligned = &node;
            break;
        }
    }
    CHECK(aligned != nullptr);
    if (aligned != nullptr) {
        CHECK(aligned->aux == 3);    // rows
        CHECK(aligned->value == 3);  // alignment columns
    }
    CHECK(count_kind(tree, nmarkdown::MathNodeKind::Tag) == 2);

    bool found_given = false;
    bool found_identity = false;
    bool found_equations = false;
    bool found_tag_one = false;
    bool found_tag_two = false;
    for (const nmarkdown::MathNode& node : tree.nodes) {
        const std::string_view text = tree.text(node);
        if (node.kind == nmarkdown::MathNodeKind::Tag) {
            found_tag_one = found_tag_one || text == "(1)";
            found_tag_two = found_tag_two || text == "(2)";
            continue;
        }
        if (node.kind != nmarkdown::MathNodeKind::Text) continue;
        found_given = found_given || text == "Given";
        found_identity = found_identity || text == "additive identity";
        // Dollar delimiters inside \text are syntax and must not leak into
        // the shaped English text.
        found_equations = found_equations || text == "equations (1) and (2)";
    }
    CHECK(found_given);
    CHECK(found_identity);
    CHECK(found_equations);
    CHECK(found_tag_one);
    CHECK(found_tag_two);
}

}  // namespace

int main() {
    test_lexer();
    test_structures_and_symbols();
    test_accents_styles_and_matrix();
    test_complete_native_symbol_catalog();
    test_recovery_and_limits();
    test_visible_space_text_command();
    test_safe_macro_aliases();
    test_markdown_formula_document_coverage();
    test_large_align_formula_structure();
    if (failures != 0) {
        std::fprintf(stderr, "%d math parser test(s) failed\n", failures);
        return 1;
    }
    std::printf("All math parser tests passed\n");
    return 0;
}
