#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "nmarkdown/app/viewer.h"
#include "nmarkdown/document/markdown.h"
#include "nmarkdown/render/primitives.h"
#include "nmarkdown/render/surface565.h"

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

bool load_markdown(nmarkdown::Viewer& viewer, const std::string& source) {
    std::unique_ptr<nmarkdown::MarkdownDocument> document(
        new nmarkdown::MarkdownDocument());
    std::string error;
    if (!nmarkdown::parse_markdown(
            reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
            *document, error)) {
        return false;
    }
    nmarkdown::DocumentProbe probe;
    probe.size = source.size();
    return viewer.set_markdown_document(std::move(document), probe, error);
}

int changed_vertical_span(const nmarkdown::Surface565& surface,
                          nmarkdown::Rect area,
                          std::uint16_t background) {
    int first = area.y + area.height;
    int last = area.y - 1;
    for (int y = area.y; y < area.y + area.height; ++y) {
        for (int x = area.x; x < area.x + area.width; ++x) {
            if (surface.pixel(x, y) == background) continue;
            first = std::min(first, y);
            last = std::max(last, y);
        }
    }
    return last < first ? 0 : last - first + 1;
}

int count_color(const nmarkdown::Surface565& surface,
                nmarkdown::Rect area,
                std::uint16_t color) {
    int count = 0;
    const nmarkdown::Rect clipped = nmarkdown::intersect(area, surface.bounds());
    for (int y = clipped.y; y < clipped.y + clipped.height; ++y) {
        for (int x = clipped.x; x < clipped.x + clipped.width; ++x) {
            if (surface.pixel(x, y) == color) ++count;
        }
    }
    return count;
}

int last_color_row(const nmarkdown::Surface565& surface,
                   nmarkdown::Rect area,
                   std::uint16_t color) {
    int last = -1;
    const nmarkdown::Rect clipped = nmarkdown::intersect(area, surface.bounds());
    for (int y = clipped.y; y < clipped.y + clipped.height; ++y) {
        for (int x = clipped.x; x < clipped.x + clipped.width; ++x) {
            if (surface.pixel(x, y) == color) last = y;
        }
    }
    return last;
}

void send_text(nmarkdown::Viewer& viewer, const char* text) {
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        CHECK(viewer.handle_event(
            {nmarkdown::InputEventType::TextInput,
             static_cast<unsigned char>(*cursor)}));
    }
}

void test_modal_scale_and_last_row_fit() {
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    const std::uint16_t accent = nmarkdown::rgb565(61, 144, 214);
    const std::uint16_t overlay = nmarkdown::rgb565(248, 249, 251);
    const std::uint16_t paper = nmarkdown::rgb565(255, 255, 252);
    const std::uint16_t selection_marker = nmarkdown::rgb565(250, 252, 255);

    nmarkdown::Viewer settings;
    CHECK(load_markdown(settings, "Settings body.\n"));
    CHECK(settings.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    settings.render(surface);
    // The twelve-row settings list keeps nine genuine 12 px rows visible and
    // scrolls instead of squeezing the menu text.
    CHECK(changed_vertical_span(surface, {40, 43, 230, 18}, accent) >= 9);
    for (int index = 0; index < 9; ++index) {
        CHECK(settings.handle_event(
            {nmarkdown::InputEventType::ScrollLineDown, 0}));
    }
    settings.render(surface);
    CHECK(surface.pixel(23, 207) == selection_marker);
    CHECK(surface.pixel(23, 222) != accent);

    nmarkdown::Viewer browser;
    CHECK(load_markdown(browser, "Browser body.\n"));
    browser.show_document_browser({"/one.md", "/two.md", "/three.md",
                                   "/four.md", "/five.md", "/six.md"});
    for (int index = 0; index < 5; ++index) {
        CHECK(browser.handle_event(
            {nmarkdown::InputEventType::ScrollLineDown, 0}));
    }
    browser.render(surface);
    CHECK(surface.pixel(31, 188) == selection_marker);
    CHECK(surface.pixel(31, 216) != accent);
    CHECK(changed_vertical_span(surface, {40, 183, 230, 23}, accent) >= 10);

    std::string headings;
    for (int index = 0; index < 6; ++index) {
        headings += "# Heading " + std::to_string(index + 1) + "\n\nText.\n\n";
    }
    nmarkdown::Viewer toc;
    CHECK(load_markdown(toc, headings));
    CHECK(toc.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    for (int index = 0; index < 5; ++index) {
        CHECK(toc.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    }
    toc.render(surface);
    CHECK(surface.pixel(31, 188) == selection_marker);
    CHECK(changed_vertical_span(surface, {40, 183, 230, 23}, accent) >= 10);

    nmarkdown::Viewer fonts;
    CHECK(load_markdown(fonts, "Fonts body.\n"));
    std::vector<nmarkdown::FontFaceCatalogEntry> font_files(3);
    for (std::size_t index = 0; index < font_files.size(); ++index) {
        font_files[index].family = "Typography glyphs " +
                                   std::to_string(index + 1);
        font_files[index].path = "/documents/font-" +
                                 std::to_string(index + 1) + ".ttf.tns";
    }
    fonts.show_font_manager(font_files, {});
    CHECK(fonts.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(fonts.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    fonts.render(surface);
    CHECK(surface.pixel(31, 110) == selection_marker);
    CHECK(changed_vertical_span(surface, {40, 105, 230, 23}, accent) >= 10);

    std::string matches;
    for (int index = 0; index < 6; ++index) {
        matches += "match result " + std::to_string(index) + ".\n\n";
    }
    nmarkdown::Viewer search;
    CHECK(load_markdown(search, matches));
    CHECK(search.handle_event({nmarkdown::InputEventType::OpenSearch, 0}));
    send_text(search, "match");
    CHECK(search.search_result_count() >= 6);
    for (int index = 0; index < 5; ++index) {
        CHECK(search.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    }
    search.render(surface);
    CHECK(surface.pixel(24, 187) == selection_marker);
    CHECK(surface.pixel(24, 218) != accent);
    CHECK(changed_vertical_span(surface, {40, 182, 240, 24}, accent) >= 9);

    nmarkdown::Viewer diagnostics;
    CHECK(load_markdown(diagnostics, "Diagnostics body.\n"));
    CHECK(diagnostics.handle_event(
        {nmarkdown::InputEventType::OpenDiagnostics, 0}));
    diagnostics.render(surface);
    CHECK(changed_vertical_span(surface, {30, 193, 260, 15}, overlay) >= 8);
    CHECK(surface.pixel(20, 222) != accent);

    nmarkdown::Viewer help;
    CHECK(load_markdown(help, "No headings here.\n"));
    CHECK(help.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    help.render(surface);
    CHECK(changed_vertical_span(surface, {56, 135, 200, 18}, overlay) >= 9);

    nmarkdown::Viewer message;
    CHECK(load_markdown(message, "Message body.\n"));
    message.show_message("CJK font needed", "Select Sarasa in Fonts");
    message.render(surface);
    CHECK(changed_vertical_span(surface, {40, 100, 240, 35}, paper) >= 9);

    nmarkdown::Viewer links;
    CHECK(load_markdown(
        links, "[first](https://one.example) and [second](https://two.example)\n"));
    links.render(surface);  // Materialize the current block before activation.
    CHECK(links.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(links.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    links.render(surface);
    CHECK(surface.pixel(31, 84) == selection_marker);
    CHECK(changed_vertical_span(surface, {40, 79, 230, 23}, accent) >= 9);
}

void test_github_style_task_checkboxes() {
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    // Dominant green sampled from GitHub's browser-native checked control.
    const std::uint16_t github_green = 0x75a9;
    const std::uint16_t selection_blue = nmarkdown::rgb565(61, 144, 214);

    nmarkdown::Viewer document;
    CHECK(load_markdown(document, "- [x] Complete\n- [ ] Pending\n"));
    document.render(surface);
    CHECK(count_color(surface, surface.bounds(), github_green) >= 10);
    // The first list line's text baseline is y=25 in the fixed 320x240
    // viewport. The checked control's last visible row must sit on it.
    CHECK(last_color_row(surface, surface.bounds(), github_green) == 25);

    nmarkdown::FontFaceCatalogEntry font;
    font.path = "/documents/SarasaFixedSC-Regular.ttf.tns";
    font.family = "Sarasa Fixed SC";
    font.has_latin = true;
    font.has_cjk = true;
    font.fixed_pitch = true;
    std::array<std::string, nmarkdown::kExternalFontRoleCount> active{};
    active[0] = font.path;
    nmarkdown::Viewer menu;
    CHECK(load_markdown(menu, "Menu checkbox background.\n"));
    menu.show_font_manager({font}, active);
    CHECK(menu.handle_event({nmarkdown::InputEventType::Activate, 0}));
    menu.render(surface);
    CHECK(count_color(surface, {24, 50, 32, 170}, github_green) >= 10);
    // The checked raster must not carry its source screenshot's white matte
    // into the blue selected row.
    CHECK(surface.pixel(35, 85) == selection_blue);
    CHECK(surface.pixel(36, 86) == selection_blue);
}

}  // namespace

int main() {
    test_modal_scale_and_last_row_fit();
    test_github_style_task_checkboxes();
    if (failures != 0) {
        std::fprintf(stderr, "%d menu layout test(s) failed\n", failures);
        return 1;
    }
    std::printf("All menu layout tests passed\n");
    return 0;
}
