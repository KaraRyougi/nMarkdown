#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "nmarkdown/app/application.h"
#include "nmarkdown/document/markdown.h"
#include "nmarkdown/document/text_encoding.h"
#include "nmarkdown/document/utf8.h"
#include "nmarkdown/platform/platform.h"
#include "nmarkdown/render/surface565.h"

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

std::vector<std::uint8_t> bytes(std::initializer_list<std::uint8_t> value) {
    return std::vector<std::uint8_t>(value);
}

void check_auto(const std::vector<std::uint8_t>& input,
                nmarkdown::TextEncoding encoding,
                std::string_view expected) {
    std::string output;
    std::string error;
    nmarkdown::TextDecodeInfo info;
    CHECK(nmarkdown::decode_text_auto(input.data(), input.size(), 1U << 20U,
                                      output, info, error));
    CHECK(error.empty());
    CHECK(info.encoding == encoding);
    CHECK(info.replacement_count == 0);
    CHECK(output == expected);
    CHECK(nmarkdown::utf8_validate(
              reinterpret_cast<const std::uint8_t*>(output.data()),
              output.size())
              .valid());

}

void test_detection_and_exact_decoding() {
    check_auto(bytes({'A', 'S', 'C', 'I', 'I', '\n'}),
               nmarkdown::TextEncoding::Utf8, "ASCII\n");
    check_auto(bytes({0xEF, 0xBB, 0xBF, 'U', 'T', 'F', '-', '8', ' ',
                      0xE4, 0xB8, 0xAD, '\n'}),
               nmarkdown::TextEncoding::Utf8, u8"UTF-8 中\n");

    // 中文，扩展：€镕\n in Windows CP936/GBK. 镕 is outside GB2312 and
    // therefore also checks the requested GBK superset rather than only GB2312.
    check_auto(bytes({0xD6, 0xD0, 0xCE, 0xC4, 0xA3, 0xAC, 0xC0, 0xA9,
                      0xD5, 0xB9, 0xA3, 0xBA, 0xA2, 0xE3, 0xE9, 0x46,
                      '\n'}),
               nmarkdown::TextEncoding::Gbk, u8"中文，扩展：€镕\n");

    const std::vector<std::uint8_t> shift_jis = {
        0x93, 0xFA, 0x96, 0x7B, 0x8C, 0xEA, 0x82, 0xA9, 0x82, 0xC8,
        0x83, 0x4A, 0x83, 0x69, 0x81, 0x41, 0x8A, 0xBF, 0x8E, 0x9A,
        0x81, 0x42, '\n'};
    check_auto(shift_jis, nmarkdown::TextEncoding::ShiftJis,
               u8"日本語かなカナ、漢字。\n");

    const std::vector<std::uint8_t> euc_jp = {
        0xC6, 0xFC, 0xCB, 0xDC, 0xB8, 0xEC, 0xA4, 0xAB, 0xA4, 0xCA,
        0xA5, 0xAB, 0xA5, 0xCA, 0xA1, 0xA2, 0xB4, 0xC1, 0xBB, 0xFA,
        0xA1, 0xA3, '\n'};
    check_auto(euc_jp, nmarkdown::TextEncoding::EucJp,
               u8"日本語かなカナ、漢字。\n");

    const std::vector<std::uint8_t> iso_2022_jp = {
        0x1B, '$', 'B', 0x46, 0x7C, 0x4B, 0x5C, 0x38, 0x6C, 0x24,
        0x2B, 0x24, 0x4A, 0x25, 0x2B, 0x25, 0x4A, 0x21, 0x22, 0x34,
        0x41, 0x3B, 0x7A, 0x21, 0x23, 0x1B, '(', 'B', '\n'};
    check_auto(iso_2022_jp, nmarkdown::TextEncoding::Iso2022Jp,
               u8"日本語かなカナ、漢字。\n");

    std::string output;
    std::string error;
    nmarkdown::TextDecodeInfo info;
    const std::vector<std::uint8_t> halfwidth = {0xCA, 0xDD, 0xB6, 0xB8};
    CHECK(nmarkdown::decode_text_as(
        halfwidth.data(), halfwidth.size(), nmarkdown::TextEncoding::ShiftJis,
        64, output, info, error));
    CHECK(output == u8"ﾊﾝｶｸ");

    // JIS X 0212 first mapped cell, through both EUC-JP and ISO-2022-JP-1.
    const std::vector<std::uint8_t> euc_jis0212 = {0x8F, 0xA2, 0xAF};
    CHECK(nmarkdown::decode_text_as(
        euc_jis0212.data(), euc_jis0212.size(),
        nmarkdown::TextEncoding::EucJp, 64, output, info, error));
    CHECK(output == u8"˘");
    const std::vector<std::uint8_t> iso_jis0212 = {
        0x1B, '$', '(', 'D', 0x22, 0x2F, 0x1B, '(', 'B'};
    CHECK(nmarkdown::decode_text_as(
        iso_jis0212.data(), iso_jis0212.size(),
        nmarkdown::TextEncoding::Iso2022Jp, 64, output, info, error));
    CHECK(output == u8"˘");
}

void test_invalid_input_and_budgets() {
    std::string output;
    std::string error;
    nmarkdown::TextDecodeInfo info;

    const std::vector<std::uint8_t> bad_bom = {0xEF, 0xBB, 0xBF, 0xFF};
    CHECK(!nmarkdown::decode_text_auto(bad_bom.data(), bad_bom.size(), 64,
                                       output, info, error));
    CHECK(error.find("invalid UTF-8") != std::string::npos);

    const std::vector<std::uint8_t> malformed_shift = {0x82, 0x20, 'A'};
    CHECK(nmarkdown::decode_text_as(
        malformed_shift.data(), malformed_shift.size(),
        nmarkdown::TextEncoding::ShiftJis, 64, output, info, error));
    CHECK(info.replacement_count == 1);
    CHECK(output.find("\xEF\xBF\xBD") != std::string::npos);

    // Reserved CP932 single bytes are intentionally not treated as private-use
    // characters. Common double-byte Windows extensions are in the table, but
    // 80/A0/FD-FF follow strict Shift-JIS invalid-sequence recovery.
    for (const std::uint8_t reserved : {0x80U, 0xA0U, 0xFDU, 0xFEU, 0xFFU}) {
        CHECK(nmarkdown::decode_text_as(
            &reserved, 1, nmarkdown::TextEncoding::ShiftJis, 8, output, info,
            error));
        CHECK(info.replacement_count == 1);
        CHECK(output == "\xEF\xBF\xBD");
    }

    const std::vector<std::uint8_t> malformed_iso = {0x1B, '$', 'X', 'A'};
    CHECK(!nmarkdown::decode_text_auto(
        malformed_iso.data(), malformed_iso.size(), 64, output, info, error));
    CHECK(error.find("ISO-2022-JP") != std::string::npos);

    const std::vector<std::uint8_t> binary = {'A', 0, 'B'};
    CHECK(!nmarkdown::decode_text_auto(binary.data(), binary.size(), 64,
                                       output, info, error));
    CHECK(error.find("binary") != std::string::npos);

    const std::vector<std::uint8_t> ascii = {'a', 'b', 'c'};
    CHECK(!nmarkdown::decode_text_auto(ascii.data(), ascii.size(), 2,
                                       output, info, error));
    CHECK(error.find("size limit") != std::string::npos);

    const std::vector<std::uint8_t> bounded_gbk = {0xD6, 0xD0};
    CHECK(!nmarkdown::decode_text_auto(
        bounded_gbk.data(), bounded_gbk.size(), 2, output, info, error));
    CHECK(error.find("size limit") != std::string::npos);
    CHECK(nmarkdown::decode_text_auto(
        bounded_gbk.data(), bounded_gbk.size(), 3, output, info, error));
    CHECK(info.encoding == nmarkdown::TextEncoding::Gbk);
    CHECK(output == u8"中");

    const std::vector<std::uint8_t> dos_eof = {'o', 'k', 0x1A};
    CHECK(nmarkdown::decode_text_auto(dos_eof.data(), dos_eof.size(), 8,
                                      output, info, error));
    CHECK(output == "ok");

    CHECK(nmarkdown::decode_text_auto(nullptr, 0, 0, output, info, error));
    CHECK(output.empty());
    CHECK(info.encoding == nmarkdown::TextEncoding::Utf8);
    CHECK(std::string(nmarkdown::text_encoding_name(
              nmarkdown::TextEncoding::Iso2022Jp)) == "ISO-2022-JP");
}

void test_all_legacy_table_boundaries() {
    std::string output;
    std::string error;
    nmarkdown::TextDecodeInfo info;

    // Every syntactically legal GBK two-byte pointer has a deterministic BMP
    // mapping in the WHATWG/CP936 index.
    std::vector<std::uint8_t> gbk;
    gbk.reserve(126U * 190U * 2U);
    for (unsigned lead = 0x81; lead <= 0xFE; ++lead) {
        for (unsigned trail = 0x40; trail <= 0xFE; ++trail) {
            if (trail == 0x7F) continue;
            gbk.push_back(static_cast<std::uint8_t>(lead));
            gbk.push_back(static_cast<std::uint8_t>(trail));
        }
    }
    CHECK(nmarkdown::decode_text_as(gbk.data(), gbk.size(),
                                    nmarkdown::TextEncoding::Gbk,
                                    gbk.size() * 2U, output, info, error));
    CHECK(info.replacement_count == 0);
    CHECK(nmarkdown::utf8_validate(
              reinterpret_cast<const std::uint8_t*>(output.data()),
              output.size())
              .valid());

    // Exercise all legal lead/trail addresses, including unmapped CP932 holes;
    // holes must become bounded replacement characters without out-of-range
    // table access.
    std::vector<std::uint8_t> shift;
    for (unsigned lead = 0x81; lead <= 0xFC; ++lead) {
        if (lead > 0x9F && lead < 0xE0) continue;
        for (unsigned trail = 0x40; trail <= 0xFC; ++trail) {
            if (trail == 0x7F) continue;
            shift.push_back(static_cast<std::uint8_t>(lead));
            shift.push_back(static_cast<std::uint8_t>(trail));
        }
    }
    CHECK(nmarkdown::decode_text_as(
        shift.data(), shift.size(), nmarkdown::TextEncoding::ShiftJis,
        shift.size() * 3U, output, info, error));
    CHECK(info.replacement_count != 0);
    CHECK(nmarkdown::utf8_validate(
              reinterpret_cast<const std::uint8_t*>(output.data()),
              output.size())
              .valid());

    std::vector<std::uint8_t> euc;
    for (unsigned row = 0xA1; row <= 0xFE; ++row) {
        for (unsigned cell = 0xA1; cell <= 0xFE; ++cell) {
            euc.push_back(static_cast<std::uint8_t>(row));
            euc.push_back(static_cast<std::uint8_t>(cell));
            euc.push_back(0x8F);
            euc.push_back(static_cast<std::uint8_t>(row));
            euc.push_back(static_cast<std::uint8_t>(cell));
        }
    }
    CHECK(nmarkdown::decode_text_as(euc.data(), euc.size(),
                                    nmarkdown::TextEncoding::EucJp,
                                    euc.size() * 2U, output, info, error));
    CHECK(nmarkdown::utf8_validate(
              reinterpret_cast<const std::uint8_t*>(output.data()),
              output.size())
              .valid());

    std::vector<std::uint8_t> iso = {0x1B, '$', 'B'};
    for (unsigned row = 0x21; row <= 0x7E; ++row) {
        for (unsigned cell = 0x21; cell <= 0x7E; ++cell) {
            iso.push_back(static_cast<std::uint8_t>(row));
            iso.push_back(static_cast<std::uint8_t>(cell));
        }
    }
    iso.insert(iso.end(), {0x1B, '$', '(', 'D'});
    for (unsigned row = 0x21; row <= 0x7E; ++row) {
        for (unsigned cell = 0x21; cell <= 0x7E; ++cell) {
            iso.push_back(static_cast<std::uint8_t>(row));
            iso.push_back(static_cast<std::uint8_t>(cell));
        }
    }
    iso.insert(iso.end(), {0x1B, '(', 'B'});
    CHECK(nmarkdown::decode_text_as(
        iso.data(), iso.size(), nmarkdown::TextEncoding::Iso2022Jp,
        iso.size() * 2U, output, info, error));
    CHECK(info.replacement_count != 0);  // Reserved JIS cells are local U+FFFD.
    CHECK(nmarkdown::utf8_validate(
              reinterpret_cast<const std::uint8_t*>(output.data()),
              output.size())
              .valid());
}

void test_plain_text_ir_is_literal_and_bounded() {
    const std::string literal =
        "# not a heading\n* not emphasis *\r\n$x$ and `code`\n";
    nmarkdown::MarkdownDocument document;
    std::string error;
    CHECK(nmarkdown::parse_plain_text(
        reinterpret_cast<const std::uint8_t*>(literal.data()), literal.size(),
        document, error));
    CHECK(document.ir.headings.empty());
    CHECK(document.ir.links.empty());
    CHECK(document.ir.blocks.size() == 1);
    std::string visible;
    std::size_t breaks = 0;
    for (const nmarkdown::InlineToken& token : document.ir.tokens) {
        CHECK(token.kind == nmarkdown::InlineKind::Text ||
              token.kind == nmarkdown::InlineKind::HardBreak);
        CHECK(token.style_flags == nmarkdown::InlineStyleNone);
        if (!token.text.empty()) {
            const std::string_view text = document.text(token.text);
            visible.append(text.data(), text.size());
        } else {
            ++breaks;
            visible.push_back('\n');
        }
    }
    CHECK(breaks == 3);
    CHECK(visible.find("# not a heading") != std::string::npos);
    CHECK(visible.find("* not emphasis *") != std::string::npos);
    CHECK(visible.find("$x$ and `code`") != std::string::npos);

    std::string long_text;
    for (int line = 0; line < 300; ++line) {
        long_text += "line " + std::to_string(line) + "\n";
    }
    CHECK(nmarkdown::parse_plain_text(
        reinterpret_cast<const std::uint8_t*>(long_text.data()),
        long_text.size(), document, error));
    CHECK(document.ir.blocks.size() == (300U + 7U) / 8U);
    CHECK(document.ir.first_block == 0);
    std::uint32_t covered = 0;
    for (std::size_t index = 0; index < document.ir.blocks.size(); ++index) {
        const nmarkdown::BlockRecord& block = document.ir.blocks[index];
        CHECK(block.kind == nmarkdown::BlockKind::Paragraph);
        CHECK((block.flags & nmarkdown::BlockFlagPlainText) != 0);
        CHECK((block.flags & nmarkdown::BlockFlagTextContinuation) ==
              (index == 0 ? 0 : nmarkdown::BlockFlagTextContinuation));
        CHECK(block.aux > 0 && block.aux <= 8);
        CHECK(block.source_offset == covered);
        covered += block.source_length;
        const nmarkdown::NodeId expected =
            index + 1 < document.ir.blocks.size()
                ? static_cast<nmarkdown::NodeId>(index + 1)
                : nmarkdown::kInvalidNode;
        CHECK(block.next_sibling == expected);
    }
    CHECK(covered == document.source.size());
}

void touch(const std::filesystem::path& path) {
    FILE* file = std::fopen(path.string().c_str(), "wb");
    CHECK(file != nullptr);
    if (file != nullptr) std::fclose(file);
}

void test_browser_filter_includes_txt_wrappers() {
    namespace fs = std::filesystem;
    const fs::path root =
        fs::path(NMARKDOWN_BINARY_DIR) / "txt-browser-filter-test";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    ignored.clear();
    CHECK(fs::create_directories(root / "nested", ignored));
    touch(root / "chapter.md");
    touch(root / "notes.TXT");
    touch(root / "nested" / "legacy.txt.tns");
    touch(root / "nested" / "book.markdown.tns");
    touch(root / "skip.rtf");
    touch(root / "skip.txt.bak");

    nmarkdown::StdioFileSystem files;
    std::vector<std::string> paths;
    std::string error;
    bool truncated = true;
    CHECK(files.list_reader_documents(root.string().c_str(), 16, paths, error,
                                      &truncated));
    CHECK(!truncated);
    CHECK(paths.size() == 4);
    const auto has_suffix = [&paths](std::string_view suffix) {
        return std::any_of(paths.begin(), paths.end(), [suffix](const auto& path) {
            return path.size() >= suffix.size() &&
                   path.compare(path.size() - suffix.size(), suffix.size(),
                                suffix) == 0;
        });
    };
    CHECK(has_suffix("chapter.md"));
    CHECK(has_suffix("notes.TXT"));
    CHECK(has_suffix("legacy.txt.tns"));
    CHECK(has_suffix("book.markdown.tns"));
    CHECK(!has_suffix("skip.rtf"));
    CHECK(!has_suffix("skip.txt.bak"));

    paths.clear();
    CHECK(files.list_reader_documents(root.string().c_str(), 1, paths, error,
                                      &truncated));
    CHECK(paths.size() == 1);
    CHECK(truncated);
    fs::remove_all(root, ignored);
}

class TxtDisplay final : public nmarkdown::Display {
public:
    bool initialize() override { return true; }
    void shutdown() override { shutdown_called = true; }
    nmarkdown::Surface565 surface() override {
        return {pixels.data(), 320, 240, 320};
    }
    void present() override { ++present_count; }

    std::vector<std::uint16_t> pixels =
        std::vector<std::uint16_t>(320U * 240U, 0);
    int present_count = 0;
    bool shutdown_called = false;
};

class ExitInput final : public nmarkdown::Input {
public:
    bool poll(nmarkdown::InputEvent& event) override {
        if (sent) return false;
        sent = true;
        event = {nmarkdown::InputEventType::Quit, 0};
        return true;
    }
    bool sent = false;
};

class TestClock final : public nmarkdown::Clock {
public:
    std::uint64_t milliseconds() const override { return ++now; }
    void sleep_ms(std::uint32_t duration) override { now += duration; }
    mutable std::uint64_t now = 0;
};

class WrappedGbkTxtFileSystem final : public nmarkdown::FileSystem {
public:
    bool probe(const char* path, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested == "notes.txt.tns") {
            result = {};
            result.size = source.size();
            return true;
        }
        error = "could not open document: No such file or directory";
        return false;
    }
    bool read_all(const char* path, std::size_t,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested == "notes.txt.tns") {
            opened_wrapped_path = true;
            data = source;
            return true;
        }
        error = "could not open document: No such file or directory";
        return false;
    }
    bool read_range(const char*, std::uint64_t, std::uint8_t*, std::size_t,
                    std::string& error) override {
        error = "unsupported";
        return false;
    }
    bool write_atomic(const char*, const std::uint8_t*, std::size_t,
                      std::string&) override {
        return true;
    }

    // "GBK text: 中文\n# literal heading\n" encoded as CP936.
    const std::vector<std::uint8_t> source = {
        'G', 'B', 'K', ' ', 't', 'e', 'x', 't', ':', ' ',
        0xD6, 0xD0, 0xCE, 0xC4, '\n', '#', ' ', 'l', 'i', 't', 'e',
        'r', 'a', 'l', ' ', 'h', 'e', 'a', 'd', 'i', 'n', 'g', '\n'};
    bool opened_wrapped_path = false;
};

void test_application_opens_wrapped_legacy_txt() {
    TxtDisplay display;
    ExitInput input;
    TestClock clock;
    WrappedGbkTxtFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = false;

    CHECK(nmarkdown::run_reader(display, input, files, clock, "notes.txt",
                                options) == 0);
    CHECK(files.opened_wrapped_path);
    CHECK(display.shutdown_called);
    CHECK(display.present_count != 0);
    const std::uint16_t failure = nmarkdown::rgb565(213, 76, 84);
    CHECK(std::count(display.pixels.begin(), display.pixels.end(), failure) < 5);
}

}  // namespace

int main() {
    test_detection_and_exact_decoding();
    test_invalid_input_and_budgets();
    test_all_legacy_table_boundaries();
    test_plain_text_ir_is_literal_and_bounded();
    test_browser_filter_includes_txt_wrappers();
    test_application_opens_wrapped_legacy_txt();

    if (failures != 0) {
        std::fprintf(stderr, "%d text-encoding test(s) failed\n", failures);
        return 1;
    }
    std::printf("All text-encoding tests passed\n");
    return 0;
}
