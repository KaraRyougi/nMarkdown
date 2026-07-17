#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include "nmarkdown/document/document_ir.h"
#include "nmarkdown/document/entity.h"
#include "nmarkdown/document/markdown.h"

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

std::size_t count_blocks(const nmarkdown::MarkdownDocument& document,
                         nmarkdown::BlockKind kind) {
    return static_cast<std::size_t>(std::count_if(
        document.ir.blocks.begin(), document.ir.blocks.end(),
        [kind](const nmarkdown::BlockRecord& block) { return block.kind == kind; }));
}

std::size_t count_tokens(const nmarkdown::MarkdownDocument& document,
                         nmarkdown::InlineKind kind) {
    return static_cast<std::size_t>(std::count_if(
        document.ir.tokens.begin(), document.ir.tokens.end(),
        [kind](const nmarkdown::InlineToken& token) { return token.kind == kind; }));
}

std::string visible_text(const nmarkdown::MarkdownDocument& document) {
    std::string result;
    for (const nmarkdown::InlineToken& token : document.ir.tokens) {
        if (!token.text.empty()) {
            const std::string_view text = document.text(token.text);
            result.append(text.data(), text.size());
        } else if (token.kind == nmarkdown::InlineKind::SoftBreak ||
                   token.kind == nmarkdown::InlineKind::HardBreak) {
            result.push_back('\n');
        }
    }
    return result;
}

void test_rich_document() {
    const char markdown[] =
        "# Heading &amp; Ω\n\n"
        "Paragraph with *emphasis*, **strong**, ~~strike~~, `code`, "
        "[a link](guide.md \"Guide\"), and $x^2$.\n\n"
        "> Quoted text\n\n"
        "- [x] finished\n"
        "- [ ] pending\n\n"
        "1. first\n"
        "2. second\n\n"
        "```cpp\nint main() { return 0; }\n```\n\n"
        "| Name | Value |\n"
        "|:-----|------:|\n"
        "| alpha | &#x3A9; |\n\n"
        "---\n";

    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(markdown),
        sizeof(markdown) - 1,
        document,
        error));
    CHECK(error.empty());
    CHECK(document.utf8.valid());
    CHECK(count_blocks(document, nmarkdown::BlockKind::Heading) == 1);
    // MD4C deliberately elides paragraph callbacks in tight lists.
    CHECK(count_blocks(document, nmarkdown::BlockKind::Paragraph) >= 2);
    CHECK(count_blocks(document, nmarkdown::BlockKind::Quote) == 1);
    CHECK(count_blocks(document, nmarkdown::BlockKind::UnorderedList) == 1);
    CHECK(count_blocks(document, nmarkdown::BlockKind::OrderedList) == 1);
    CHECK(count_blocks(document, nmarkdown::BlockKind::ListItem) == 4);
    CHECK(count_blocks(document, nmarkdown::BlockKind::CodeBlock) == 1);
    CHECK(count_blocks(document, nmarkdown::BlockKind::Table) == 1);
    CHECK(count_blocks(document, nmarkdown::BlockKind::TableRow) == 2);
    CHECK(count_blocks(document, nmarkdown::BlockKind::TableCell) == 4);
    CHECK(count_blocks(document, nmarkdown::BlockKind::HorizontalRule) == 1);
    CHECK(count_tokens(document, nmarkdown::InlineKind::EmphasisStart) == 1);
    CHECK(count_tokens(document, nmarkdown::InlineKind::StrongStart) == 1);
    CHECK(count_tokens(document, nmarkdown::InlineKind::StrikethroughStart) == 1);
    CHECK(count_tokens(document, nmarkdown::InlineKind::Code) >= 2);
    CHECK(count_tokens(document, nmarkdown::InlineKind::InlineMath) == 1);
    CHECK(document.ir.links.size() == 1);
    CHECK(document.ir.headings.size() == 1);
    CHECK(document.text(document.ir.headings[0].title) == "Heading & Ω");
    CHECK(document.ir.first_block != nmarkdown::kInvalidNode);

    const std::string text = visible_text(document);
    CHECK(text.find("Heading & Ω") != std::string::npos);
    CHECK(text.find("alpha") != std::string::npos);
    CHECK(text.find("Ω") != std::string::npos);
    CHECK(text.find("&amp;") == std::string::npos);
    CHECK(document.text(document.ir.links[0].target) == "guide.md");
    CHECK(document.text(document.ir.links[0].title) == "Guide");
    bool linked_text_has_id = false;
    for (const nmarkdown::InlineToken& token : document.ir.tokens) {
        if ((token.style_flags & nmarkdown::InlineStyleLink) != 0 &&
            token.aux == 0 && !token.text.empty()) {
            linked_text_has_id = true;
        }
    }
    CHECK(linked_text_has_id);

    bool found_checked = false;
    bool found_pending = false;
    for (const nmarkdown::BlockRecord& block : document.ir.blocks) {
        if (block.kind != nmarkdown::BlockKind::ListItem) continue;
        if ((block.flags & nmarkdown::BlockFlagTask) != 0 &&
            (block.flags & nmarkdown::BlockFlagChecked) != 0) {
            found_checked = true;
        }
        if ((block.flags & nmarkdown::BlockFlagTask) != 0 &&
            (block.flags & nmarkdown::BlockFlagChecked) == 0) {
            found_pending = true;
        }
    }
    CHECK(found_checked && found_pending);
}

void test_invalid_utf8_is_local() {
    const std::uint8_t markdown[] = {'#', ' ', 'A', 0xE2, '(', 0xA1, '\n'};
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(markdown, sizeof(markdown), document, error));
    CHECK(document.utf8.invalid_sequence_count == 2);
    CHECK(document.ir.headings.size() == 1);
    CHECK(visible_text(document).find("\xEF\xBF\xBD") != std::string::npos);
}

void test_entities() {
    std::string decoded;
    CHECK(nmarkdown::decode_html_entity("&NotEqualTilde;", decoded));
    CHECK(decoded == "\xE2\x89\x82\xCC\xB8");
    CHECK(nmarkdown::decode_html_entity("&#128;", decoded));
    CHECK(decoded == "\xE2\x82\xAC");
    CHECK(nmarkdown::decode_html_entity("&#x110000;", decoded));
    CHECK(decoded == "\xEF\xBF\xBD");
    CHECK(!nmarkdown::decode_html_entity("&doesnotexist;", decoded));
}

void test_math_source_slices() {
    const char markdown[] =
        "Inline $\\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}$.\n\n"
        "Spaced $ E = mc^2 $.\n\n"
        "Adjacent $a$或者$b$.\n\n"
        "$$\\begin{matrix}a&b\\\\c&d\\end{matrix}$$\n";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(markdown), sizeof(markdown) - 1,
        document, error));
    bool inline_found = false;
    bool display_found = false;
    bool spaced_found = false;
    bool adjacent_a = false;
    bool adjacent_b = false;
    for (const nmarkdown::InlineToken& token : document.ir.tokens) {
        if (token.kind == nmarkdown::InlineKind::InlineMath) {
            inline_found = true;
            const std::string_view formula = document.text(token.text);
            adjacent_a = adjacent_a || formula == "a";
            adjacent_b = adjacent_b || formula == "b";
            spaced_found = spaced_found || formula == " E = mc^2 ";
            CHECK(formula == "\\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}" ||
                  formula == " E = mc^2 " || formula == "a" || formula == "b");
        }
        if (token.kind == nmarkdown::InlineKind::DisplayMath) {
            display_found = true;
            CHECK(document.text(token.text) ==
                  "\\begin{matrix}a&b\\\\c&d\\end{matrix}");
        }
    }
    CHECK(inline_found && display_found && spaced_found && adjacent_a && adjacent_b);
    CHECK(count_tokens(document, nmarkdown::InlineKind::InlineMath) == 4);
}

void test_multiline_display_math_is_one_formula() {
    const char markdown[] =
        "Before.\n\n"
        "$$\n"
        "\\begin{aligned}\n"
        "a &= \\frac{1}{2} \\\\\n"
        "b &= \\text{if $n$ is even}\n"
        "\\end{aligned}\n"
        "$$\n\n"
        "After.\n";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(markdown), sizeof(markdown) - 1,
        document, error));
    CHECK(count_tokens(document, nmarkdown::InlineKind::DisplayMath) == 1);
    for (const nmarkdown::InlineToken& token : document.ir.tokens) {
        if (token.kind != nmarkdown::InlineKind::DisplayMath) continue;
        const std::string_view formula = document.text(token.text);
        CHECK(formula.find("\\begin{aligned}") != std::string_view::npos);
        CHECK(formula.find("a &= \\frac{1}{2}") != std::string_view::npos);
        CHECK(formula.find("b &= \\text{if $n$ is even}") !=
              std::string_view::npos);
        CHECK(formula.find("\\end{aligned}") != std::string_view::npos);
        CHECK(token.source_length > 0);
        CHECK(token.text.storage == nmarkdown::TextStorageKind::Owned);
    }
    for (const nmarkdown::InlineToken& token : document.ir.tokens) {
        if (token.kind == nmarkdown::InlineKind::Text) {
            CHECK(document.text(token.text).find('$') == std::string_view::npos);
        }
    }
}

}  // namespace

int main() {
    test_rich_document();
    test_invalid_utf8_is_local();
    test_entities();
    test_math_source_slices();
    test_multiline_display_math_is_one_formula();
    if (failures != 0) {
        std::fprintf(stderr, "%d Markdown test(s) failed\n", failures);
        return 1;
    }
    std::printf("All Markdown tests passed\n");
    return 0;
}
