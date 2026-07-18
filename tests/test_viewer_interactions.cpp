#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "nmarkdown/app/viewer.h"
#include "nmarkdown/document/markdown.h"
#include "nmarkdown/document/utf8.h"
#include "nmarkdown/io/memory_random_access.h"
#include "nmarkdown/render/surface565.h"

namespace {

int failures = 0;
const char* current_case = "viewer interactions";

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::fprintf(stderr,
                 "CHECK failed in %s at %s:%d: %s\n",
                 current_case,
                 __FILE__,
                 line,
                 expression);
    ++failures;
}

#define CHECK(condition) check((condition), #condition, __LINE__)

using SetupOverlay = std::function<bool(nmarkdown::Viewer&)>;

struct OverlayCase {
    const char* name;
    SetupOverlay setup;
};

class ImmediateClock final : public nmarkdown::Clock {
public:
    std::uint64_t milliseconds() const override { return 0; }
    void sleep_ms(std::uint32_t) override {}
};

bool send(nmarkdown::Viewer& viewer,
          nmarkdown::InputEventType type,
          int amount = 0) {
    return viewer.handle_event({type, amount});
}

bool send_numeric_alias(nmarkdown::Viewer& viewer,
                        nmarkdown::InputEventType type,
                        char digit) {
    return viewer.handle_event(
        {type, digit, nmarkdown::InputEventOrigin::NumericNavigationAlias});
}

bool tap(nmarkdown::Viewer& viewer) {
    return viewer.handle_event(
        {nmarkdown::InputEventType::Activate, 0,
         nmarkdown::InputEventOrigin::TouchpadTap});
}

bool click(nmarkdown::Viewer& viewer) {
    return viewer.handle_event(
        {nmarkdown::InputEventType::Activate, 0,
         nmarkdown::InputEventOrigin::TouchpadActivation});
}

bool load_markdown(nmarkdown::Viewer& viewer, const std::string& source) {
    auto document = std::make_unique<nmarkdown::MarkdownDocument>();
    std::string error;
    if (!nmarkdown::parse_markdown(
            reinterpret_cast<const std::uint8_t*>(source.data()),
            source.size(), *document, error)) {
        std::fprintf(stderr, "Markdown setup failed: %s\n", error.c_str());
        return false;
    }
    nmarkdown::DocumentProbe probe;
    probe.size = source.size();
    return viewer.set_markdown_document(std::move(document), probe, error);
}

bool load_plain_text(nmarkdown::Viewer& viewer, std::string source) {
    const nmarkdown::Utf8ValidationResult validation =
        nmarkdown::utf8_validate(
            reinterpret_cast<const std::uint8_t*>(source.data()),
            source.size(), false);
    const std::uint32_t size = static_cast<std::uint32_t>(source.size());
    auto data = std::make_shared<nmarkdown::MemoryRandomAccessData>(
        std::move(source));
    nmarkdown::DocumentProbe probe;
    probe.size = size;
    std::string error;
    return viewer.set_plain_text_document(
        std::move(data), 0, size, validation, probe, error);
}

bool open_settings(nmarkdown::Viewer& viewer) {
    return send(viewer, nmarkdown::InputEventType::OpenSettings);
}

bool select_code_block_pan_mode(nmarkdown::Viewer& viewer) {
    if (!viewer.reader_state(0).code_wrap) return true;
    if (!open_settings(viewer)) return false;
    for (int row = 0; row < 5; ++row) {
        if (!send(viewer, nmarkdown::InputEventType::ScrollLineDown)) return false;
    }
    return send(viewer, nmarkdown::InputEventType::PanRight) &&
           send(viewer, nmarkdown::InputEventType::Activate) &&
           !viewer.reader_state(0).code_wrap;
}

bool open_help(nmarkdown::Viewer& viewer) {
    return load_markdown(viewer, "Reader body without headings.\n") &&
           send(viewer, nmarkdown::InputEventType::OpenMenu);
}

bool open_toc(nmarkdown::Viewer& viewer) {
    return load_markdown(viewer, "# First\n\nBody.\n\n## Second\n\nMore.\n") &&
           send(viewer, nmarkdown::InputEventType::OpenMenu);
}

bool open_bookmarks(nmarkdown::Viewer& viewer) {
    if (!load_markdown(viewer, "# First\n\nBody.\n\n## Second\n\nMore.\n")) {
        return false;
    }
    if (!send(viewer, nmarkdown::InputEventType::ToggleBookmark) ||
        !send(viewer, nmarkdown::InputEventType::OpenMenu)) {
        return false;
    }
    return send(viewer, nmarkdown::InputEventType::PanRight);
}

bool open_search(nmarkdown::Viewer& viewer) {
    return load_markdown(viewer, "Searchable reader body.\n") &&
           send(viewer, nmarkdown::InputEventType::OpenSearch);
}

bool open_diagnostics(nmarkdown::Viewer& viewer) {
    return send(viewer, nmarkdown::InputEventType::OpenDiagnostics);
}

bool open_loaded_document_browser(nmarkdown::Viewer& viewer) {
    nmarkdown::DocumentProbe probe;
    probe.size = 512;
    viewer.set_document(probe);
    viewer.show_document_browser({"/documents/one.md.tns"});
    return true;
}

bool open_startup_document_browser(nmarkdown::Viewer& viewer) {
    viewer.show_document_browser({"/documents/one.md.tns"});
    return true;
}

nmarkdown::FontFaceCatalogEntry test_font(std::string name,
                                          std::string path,
                                          bool has_cjk = true) {
    nmarkdown::FontFaceCatalogEntry font;
    font.family = std::move(name);
    font.path = std::move(path);
    font.has_latin = true;
    font.has_cjk = has_cjk;
    return font;
}

bool open_font_role_menu(nmarkdown::Viewer& viewer) {
    viewer.show_font_manager(
        {test_font("Reader", "/documents/fonts/reader.ttf.tns")}, {});
    return true;
}

bool open_font_file_menu(nmarkdown::Viewer& viewer) {
    viewer.show_font_manager(
        {test_font("Reader", "/documents/fonts/reader.ttf.tns")}, {});
    return send(viewer, nmarkdown::InputEventType::Activate);
}

bool open_message(nmarkdown::Viewer& viewer) {
    viewer.show_message("Reader notice", "Nothing was changed.");
    return true;
}

bool open_multi_link_chooser(nmarkdown::Viewer& viewer) {
    return load_markdown(
               viewer,
               "[First](one.md) [Second](two.md) [Third](three.md)\n") &&
           send(viewer, nmarkdown::InputEventType::Activate);
}

std::vector<OverlayCase> overlay_cases() {
    return {
        {"Settings", open_settings},
        {"Help", open_help},
        {"TOC", open_toc},
        {"Bookmarks", open_bookmarks},
        {"Search", open_search},
        {"Diagnostics", open_diagnostics},
        {"document browser (loaded)", open_loaded_document_browser},
        {"document browser (startup)", open_startup_document_browser},
        {"font manager", open_font_role_menu},
        {"font detail", open_font_file_menu},
        {"message dialog", open_message},
        {"multi-link chooser", open_multi_link_chooser},
    };
}

void test_quit_has_global_priority() {
    for (const OverlayCase& overlay : overlay_cases()) {
        current_case = overlay.name;
        nmarkdown::Viewer viewer;
        CHECK(overlay.setup(viewer));
        CHECK(!viewer.quit_requested());
        CHECK(send(viewer, nmarkdown::InputEventType::Quit));
        CHECK(viewer.quit_requested());
    }
}

void expect_single_layer_back(const OverlayCase& overlay) {
    current_case = overlay.name;
    nmarkdown::Viewer viewer;
    CHECK(overlay.setup(viewer));
    CHECK(send(viewer, nmarkdown::InputEventType::Back));
    CHECK(!viewer.quit_requested());
    send(viewer, nmarkdown::InputEventType::Back);
    CHECK(viewer.quit_requested());
}

void test_back_closes_visible_layer_before_reader() {
    const std::vector<OverlayCase> single_layer_cases{
        {"Settings Back stack", open_settings},
        {"Help Back stack", open_help},
        {"TOC Back stack", open_toc},
        {"Bookmarks Back stack", open_bookmarks},
        {"Search Back stack", open_search},
        {"Diagnostics Back stack", open_diagnostics},
        {"loaded document browser Back stack", open_loaded_document_browser},
        {"font role Back stack", open_font_role_menu},
        {"message Back stack", open_message},
        {"multi-link Back stack", open_multi_link_chooser},
    };
    for (const OverlayCase& overlay : single_layer_cases) {
        expect_single_layer_back(overlay);
    }

    current_case = "startup document browser Back exits";
    nmarkdown::Viewer startup_browser;
    CHECK(open_startup_document_browser(startup_browser));
    CHECK(send(startup_browser, nmarkdown::InputEventType::Back));
    CHECK(startup_browser.quit_requested());

    current_case = "font file menu has two visible Back layers";
    nmarkdown::Viewer font_browser;
    CHECK(open_font_file_menu(font_browser));
    CHECK(send(font_browser, nmarkdown::InputEventType::Back));
    CHECK(!font_browser.quit_requested());
    CHECK(send(font_browser, nmarkdown::InputEventType::Back));
    CHECK(!font_browser.quit_requested());
    send(font_browser, nmarkdown::InputEventType::Back);
    CHECK(font_browser.quit_requested());
}

struct DocumentSnapshot {
    int scroll_y = 0;
    int pan_x = 0;
    int page = 0;
    int total_pages = 0;
    int body_size = 0;
    bool dark_theme = false;
    std::size_t bookmark_count = 0;
};

DocumentSnapshot snapshot(const nmarkdown::Viewer& viewer) {
    DocumentSnapshot value;
    value.scroll_y = viewer.scroll_y();
    value.pan_x = viewer.pan_x();
    value.page = viewer.current_page();
    value.total_pages = viewer.total_pages();
    value.body_size = viewer.body_pixel_size();
    value.dark_theme = viewer.dark_theme();
    value.bookmark_count = viewer.reader_state(1).bookmarks.size();
    return value;
}

void expect_same_document_state(const DocumentSnapshot& before,
                                const nmarkdown::Viewer& viewer) {
    CHECK(viewer.scroll_y() == before.scroll_y);
    CHECK(viewer.pan_x() == before.pan_x);
    CHECK(viewer.current_page() == before.page);
    CHECK(viewer.total_pages() == before.total_pages);
    CHECK(viewer.body_pixel_size() == before.body_size);
    CHECK(viewer.dark_theme() == before.dark_theme);
    CHECK(viewer.reader_state(1).bookmarks.size() == before.bookmark_count);
}

std::string long_document_without_headings() {
    std::string source =
        "```cpp\n"
        "const char* wide = \"a deliberately long code line that requires horizontal panning on a 320 pixel display\";\n"
        "```\n\n";
    for (int index = 0; index < 72; ++index) {
        source += "A long body paragraph keeps the reader scrollable while a modal layer is visible.\n\n";
    }
    return source;
}

void exercise_passive_help_isolation(nmarkdown::ReadingMode mode,
                                     const char* label) {
    current_case = label;
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, long_document_without_headings()));
    viewer.set_reading_mode(mode);

    if (mode == nmarkdown::ReadingMode::VerticalScroll) {
        CHECK(select_code_block_pan_mode(viewer));
        CHECK(send(viewer, nmarkdown::InputEventType::Activate));
        CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
        CHECK(viewer.pan_x() > 0);
        CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
        CHECK(viewer.scroll_y() > 0);
    } else {
        CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
        CHECK(viewer.current_page() > 1);
    }

    CHECK(send(viewer, nmarkdown::InputEventType::OpenMenu));
    const DocumentSnapshot before = snapshot(viewer);
    const std::array<nmarkdown::InputEvent, 15> covered_shortcuts{{
        {nmarkdown::InputEventType::ScrollLineUp, 0},
        {nmarkdown::InputEventType::ScrollLineDown, 0},
        {nmarkdown::InputEventType::PageUp, 0},
        {nmarkdown::InputEventType::PageDown, 0},
        {nmarkdown::InputEventType::SwipeLeft, 0},
        {nmarkdown::InputEventType::SwipeRight, 0},
        {nmarkdown::InputEventType::SwipeUp, 0},
        {nmarkdown::InputEventType::SwipeDown, 0},
        {nmarkdown::InputEventType::PanLeft, 0},
        {nmarkdown::InputEventType::PanRight, 0},
        {nmarkdown::InputEventType::PointerScroll, 90},
        {nmarkdown::InputEventType::PointerPan, 90},
        {nmarkdown::InputEventType::IncreaseFont, 0},
        {nmarkdown::InputEventType::ToggleBookmark, 0},
        {nmarkdown::InputEventType::Activate, 0},
    }};
    for (const nmarkdown::InputEvent& event : covered_shortcuts) {
        viewer.handle_event(event);
        expect_same_document_state(before, viewer);
    }

    CHECK(send(viewer, nmarkdown::InputEventType::Back));
    CHECK(!viewer.quit_requested());
}

void test_passive_overlay_isolates_both_touchpad_modes() {
    exercise_passive_help_isolation(nmarkdown::ReadingMode::VerticalScroll,
                                    "Vertical Scroll modal isolation");
    exercise_passive_help_isolation(nmarkdown::ReadingMode::HorizontalScroll,
                                    "Horizontal Scroll modal isolation");
}

void test_non_owning_shortcuts_do_not_leak_through_overlay_matrix() {
    // Continuous motion remains inert over every modal. Named swipes are
    // intentionally excluded: modal lists now own them for navigation.
    const std::array<nmarkdown::InputEvent, 4> non_owning_shortcuts{{
        {nmarkdown::InputEventType::PointerScroll, 75},
        {nmarkdown::InputEventType::PointerPan, 75},
        {nmarkdown::InputEventType::IncreaseFont, 0},
        {nmarkdown::InputEventType::ToggleBookmark, 0},
    }};
    for (const OverlayCase& overlay : overlay_cases()) {
        current_case = overlay.name;
        nmarkdown::Viewer viewer;
        CHECK(overlay.setup(viewer));
        const DocumentSnapshot before = snapshot(viewer);
        for (const nmarkdown::InputEvent& event : non_owning_shortcuts) {
            viewer.handle_event(event);
            expect_same_document_state(before, viewer);
        }
    }
}

void test_search_treats_navigation_digits_as_query_text() {
    current_case = "numeric navigation aliases become Search text";
    std::string source = "12467828 first searchable row.\n\n"
                         "12467828 second searchable row.\n\n";
    for (int index = 0; index < 40; ++index) {
        source += "Filler keeps normal document navigation observable.\n\n";
    }

    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    CHECK(send(viewer, nmarkdown::InputEventType::OpenSearch));

    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::PageDown,
                             '1'));
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::ScrollLineDown,
                             '2'));
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::PanLeft, '4'));
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::PanRight, '6'));
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::PageUp, '7'));
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::ScrollLineUp,
                             '8'));
    // Shift/Ctrl + 2/8 preserve their numeric identity even though their
    // reader events are PageDown/PageUp.
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::PageDown, '2'));
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::PageUp, '8'));
    CHECK(viewer.search_query() == "12467828");
    CHECK(viewer.search_result_count() == 2);

    // Real arrows retain Search navigation/mode behavior and never append.
    CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
    CHECK(viewer.search_query() == "12467828");
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(viewer.search_query() == "12467828");

    CHECK(send(viewer, nmarkdown::InputEventType::Back));
    CHECK(viewer.scroll_y() == 0);
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::ScrollLineDown,
                             '2'));
    CHECK(viewer.scroll_y() > 0);
}

void test_modal_swipes_and_touchpad_activation() {
    current_case = "settings ignores swipes and touchpad tap";
    {
        nmarkdown::Viewer viewer;
        const int initial_size = viewer.body_pixel_size();
        CHECK(open_settings(viewer));
        // No swipe may move the selected row or change its value.
        CHECK(!send(viewer, nmarkdown::InputEventType::SwipeUp));
        CHECK(!send(viewer, nmarkdown::InputEventType::SwipeDown));
        CHECK(!send(viewer, nmarkdown::InputEventType::SwipeLeft));
        CHECK(!send(viewer, nmarkdown::InputEventType::SwipeRight));
        CHECK(viewer.body_pixel_size() == initial_size);
        // Keyboard navigation remains available; touchpad tap does not
        // confirm or alter a setting. Left/Right edits, while Enter applies
        // and closes without changing the value again.
        CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
        CHECK(!tap(viewer));
        CHECK(viewer.body_pixel_size() == initial_size);
        CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
        CHECK(viewer.body_pixel_size() == initial_size + 1);
        CHECK(send(viewer, nmarkdown::InputEventType::Activate));
        CHECK(viewer.body_pixel_size() == initial_size + 1);
        CHECK(viewer.reader_state(0).font_size == initial_size + 1);
    }

    current_case = "TOC ignores tap and accepts click";
    {
        std::string source;
        for (int index = 0; index < 9; ++index) {
            source += "# Heading " + std::to_string(index + 1) + "\n\n";
            for (int line = 0; line < 6; ++line) {
                source += "Filler under this heading keeps later sections below the first viewport.\n\n";
            }
        }
        nmarkdown::Viewer viewer;
        CHECK(load_markdown(viewer, source));
        CHECK(send(viewer, nmarkdown::InputEventType::OpenMenu));
        CHECK(send(viewer, nmarkdown::InputEventType::SwipeUp));
        CHECK(!tap(viewer));
        CHECK(viewer.scroll_y() == 0);
        CHECK(click(viewer));
        CHECK(viewer.scroll_y() > 0);
    }

    current_case = "document browser ignores tap and accepts click";
    {
        std::vector<std::string> paths;
        for (int index = 0; index < 8; ++index) {
            paths.push_back("/documents/book-" + std::to_string(index) +
                            ".md.tns");
        }
        nmarkdown::Viewer viewer;
        nmarkdown::DocumentProbe probe;
        probe.size = 100;
        viewer.set_document(probe);
        viewer.show_document_browser(paths);
        CHECK(send(viewer, nmarkdown::InputEventType::SwipeUp));
        std::string selected;
        CHECK(!tap(viewer));
        CHECK(!viewer.take_document_open_request(selected));
        CHECK(click(viewer));
        CHECK(viewer.take_document_open_request(selected));
        CHECK(selected == paths[5]);
    }

    current_case = "font menus ignore tap and accept click";
    {
        std::vector<nmarkdown::FontFaceCatalogEntry> fonts;
        for (int index = 0; index < 7; ++index) {
            fonts.push_back(test_font(
                "Family " + std::to_string(index),
                "/documents/fonts/font-" + std::to_string(index) +
                    ".ttf.tns"));
        }
        nmarkdown::Viewer viewer;
        viewer.show_font_manager(fonts, {});
        // One screen-step reaches the fifth installed font.
        CHECK(send(viewer, nmarkdown::InputEventType::SwipeUp));
        CHECK(!tap(viewer));
        CHECK(click(viewer));
        // Detail starts on Body. Assign it, return, then apply the staged map.
        CHECK(!tap(viewer));
        CHECK(click(viewer));
        CHECK(send(viewer, nmarkdown::InputEventType::Back));
        CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
        CHECK(!tap(viewer));
        CHECK(click(viewer));
        std::array<std::string, nmarkdown::kExternalFontRoleCount> selected;
        CHECK(viewer.take_font_assignments(selected));
        CHECK(selected[0] == fonts[5].path);
    }

    current_case = "Search result ignores tap and accepts click";
    {
        std::string source;
        for (int index = 0; index < 10; ++index) {
            source += "needle result " + std::to_string(index) +
                      " is deliberately separated from the next one.\n\n";
        }
        nmarkdown::Viewer viewer;
        CHECK(load_markdown(viewer, source));
        CHECK(send(viewer, nmarkdown::InputEventType::OpenSearch));
        for (const char ch : std::string("needle")) {
            CHECK(send(viewer, nmarkdown::InputEventType::TextInput, ch));
        }
        CHECK(viewer.search_result_count() == 10);
        CHECK(send(viewer, nmarkdown::InputEventType::SwipeUp));
        CHECK(!tap(viewer));
        CHECK(!viewer.has_active_search_match());
        CHECK(click(viewer));
        CHECK(viewer.has_active_search_match());
        CHECK(viewer.scroll_y() > 0);
    }

    current_case = "link chooser ignores tap and accepts click";
    {
        nmarkdown::Viewer viewer;
        CHECK(load_markdown(
            viewer,
            "[One](one.md) [Two](two.md) [Three](three.md) "
            "[Four](four.md) [Five](five.md) [Six](six.md)\n"));
        CHECK(send(viewer, nmarkdown::InputEventType::Activate));
        CHECK(send(viewer, nmarkdown::InputEventType::SwipeUp));
        std::string target;
        CHECK(!tap(viewer));
        CHECK(!viewer.take_document_link_request(target));
        CHECK(click(viewer));
        CHECK(viewer.take_document_link_request(target));
        CHECK(target == "six.md");
    }

    current_case = "passive overlays ignore tap and accept click";
    {
        nmarkdown::Viewer keyboard;
        CHECK(open_diagnostics(keyboard));
        CHECK(!send(keyboard, nmarkdown::InputEventType::Activate));
        CHECK(send(keyboard, nmarkdown::InputEventType::Back));
        CHECK(!keyboard.quit_requested());

        nmarkdown::Viewer diagnostics;
        CHECK(open_diagnostics(diagnostics));
        CHECK(!tap(diagnostics));
        CHECK(click(diagnostics));
        send(diagnostics, nmarkdown::InputEventType::Back);
        CHECK(diagnostics.quit_requested());

        nmarkdown::Viewer help;
        CHECK(open_help(help));
        CHECK(!tap(help));
        CHECK(click(help));
        send(help, nmarkdown::InputEventType::Back);
        CHECK(help.quit_requested());

        nmarkdown::Viewer message;
        CHECK(open_message(message));
        CHECK(!tap(message));
        CHECK(click(message));
        send(message, nmarkdown::InputEventType::Back);
        CHECK(message.quit_requested());
    }
}

void test_touchpad_modes_and_gesture_direction() {
    current_case = "touchpad axes and independent gesture directions";
    nmarkdown::Viewer viewer;
    // A deliberately narrow document: horizontal input over wide content is
    // reserved for panning, and this test is about gesture-direction
    // mapping, so no block may engage the pan reservation.
    std::string plain_source;
    for (int index = 0; index < 72; ++index) {
        plain_source +=
            "A long body paragraph keeps the reader scrollable while a "
            "modal layer is visible.\n\n";
    }
    CHECK(load_markdown(viewer, plain_source));
    CHECK(viewer.reading_mode() == nmarkdown::ReadingMode::VerticalScroll);
    CHECK(viewer.natural_scrolling());
    CHECK(viewer.natural_swiping());

    // Keys keep conventional directions regardless of the touchpad setting.
    CHECK(!send(viewer, nmarkdown::InputEventType::ScrollLineUp));
    CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
    const int line_position = viewer.scroll_y();
    CHECK(line_position > 0);
    CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineUp));
    CHECK(viewer.scroll_y() < line_position);
    CHECK(send(viewer, nmarkdown::InputEventType::PageUp));
    CHECK(viewer.scroll_y() == 0);
    CHECK(!send(viewer, nmarkdown::InputEventType::PageUp));

    CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
    const int screen_step_position = viewer.scroll_y();
    CHECK(screen_step_position > line_position);
    CHECK(send(viewer, nmarkdown::InputEventType::PageUp));
    CHECK(viewer.scroll_y() == 0);

    CHECK(send(viewer, nmarkdown::InputEventType::PointerScroll, -24));
    CHECK(viewer.scroll_y() > 0);
    CHECK(send(viewer, nmarkdown::InputEventType::PointerScroll, 24));
    CHECK(viewer.scroll_y() == 0);

    // In Natural mode, the discrete gesture follows reading order: a
    // left-to-right swipe advances one screen.
    CHECK(send(viewer, nmarkdown::InputEventType::SwipeRight));
    CHECK(viewer.scroll_y() > 0);
    CHECK(send(viewer, nmarkdown::InputEventType::SwipeLeft));
    CHECK(viewer.scroll_y() == 0);

    // Move to Swipe gesture direction (row 9) and select Reversed. Continuous
    // scrolling remains Natural and the covered document does not move.
    CHECK(open_settings(viewer));
    for (int row = 0; row < 9; ++row) {
        CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
    }
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    CHECK(!viewer.natural_swiping());
    CHECK(!viewer.reader_state(0).natural_swiping);
    CHECK(viewer.natural_scrolling());
    CHECK(viewer.reader_state(0).natural_scrolling);
    CHECK(send(viewer, nmarkdown::InputEventType::PointerScroll, -24));
    CHECK(viewer.scroll_y() > 0);
    CHECK(send(viewer, nmarkdown::InputEventType::PointerScroll, 24));
    CHECK(viewer.scroll_y() == 0);

    // Vertical Scroll ignores vertical threshold markers because PointerScroll
    // owns that continuous axis. Its horizontal swipe performs a screen step.
    CHECK(!send(viewer, nmarkdown::InputEventType::SwipeUp));
    CHECK(!send(viewer, nmarkdown::InputEventType::SwipeDown));
    CHECK(!send(viewer, nmarkdown::InputEventType::PointerPan, 24));
    CHECK(viewer.scroll_y() == 0);

    CHECK(send(viewer, nmarkdown::InputEventType::SwipeLeft));
    const int swipe_position = viewer.scroll_y();
    CHECK(swipe_position > 0);
    CHECK(send(viewer, nmarkdown::InputEventType::SwipeRight));
    CHECK(viewer.scroll_y() == 0);

    // Horizontal Scroll keeps Up/Down as literal line scrolling and makes the
    // vertical swipe axis perform boundary-aligned screen steps. Swipe is
    // Reversed while continuous scrolling remains Natural.
    viewer.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);

    CHECK(!send(viewer, nmarkdown::InputEventType::ScrollLineUp));
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::ScrollLineDown,
                             '2'));
    const int horizontal_line_position = viewer.scroll_y();
    CHECK(horizontal_line_position > 0);
    CHECK(viewer.current_page() == 1);
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::ScrollLineUp,
                             '8'));
    CHECK(viewer.scroll_y() < horizontal_line_position);
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::PageUp, '8'));
    CHECK(viewer.scroll_y() == 0);
    CHECK(!send_numeric_alias(viewer, nmarkdown::InputEventType::PageUp, '8'));
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::PageDown, '2'));
    const int keyed_step = viewer.scroll_y();
    CHECK(keyed_step > 0);
    CHECK(send_numeric_alias(viewer, nmarkdown::InputEventType::PageUp, '8'));
    CHECK(viewer.scroll_y() == 0);

    CHECK(send(viewer, nmarkdown::InputEventType::SwipeUp));
    const int reversed_swipe_step = viewer.scroll_y();
    CHECK(reversed_swipe_step > 0);
    CHECK(send(viewer, nmarkdown::InputEventType::SwipeDown));
    CHECK(viewer.scroll_y() == 0);

    // Horizontal Scroll owns PointerPan. Horizontal threshold markers and the
    // vertical delta stream are inert so one physical gesture is applied once.
    CHECK(!send(viewer, nmarkdown::InputEventType::SwipeLeft));
    CHECK(!send(viewer, nmarkdown::InputEventType::SwipeRight));
    CHECK(!send(viewer, nmarkdown::InputEventType::PointerScroll, 24));
    CHECK(viewer.scroll_y() == 0);
    CHECK(send(viewer, nmarkdown::InputEventType::PointerPan, -24));
    CHECK(viewer.scroll_y() == 24);
    CHECK(send(viewer, nmarkdown::InputEventType::PointerPan, 24));
    CHECK(viewer.scroll_y() == 0);

    // Scroll gesture direction is the next row and can be reversed without
    // changing the retained swipe direction.
    CHECK(open_settings(viewer));
    CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    CHECK(!viewer.natural_scrolling());
    CHECK(!viewer.natural_swiping());
    CHECK(send(viewer, nmarkdown::InputEventType::PointerPan, 24));
    CHECK(viewer.scroll_y() == 24);
    CHECK(send(viewer, nmarkdown::InputEventType::PointerPan, -24));
    CHECK(viewer.scroll_y() == 0);

    CHECK(send(viewer, nmarkdown::InputEventType::SwipeUp));
    CHECK(viewer.scroll_y() > 0);
    CHECK(send(viewer, nmarkdown::InputEventType::SwipeDown));
    CHECK(viewer.scroll_y() == 0);

    // Keys remain semantic and never inherit the gesture-direction setting:
    // Left is earlier, Right is later, and an immediate reverse is exact.
    CHECK(!send(viewer, nmarkdown::InputEventType::PanLeft));
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    const int right_key_step = viewer.scroll_y();
    CHECK(right_key_step > 0);
    CHECK(send(viewer, nmarkdown::InputEventType::PanLeft));
    CHECK(viewer.scroll_y() == 0);
}

void test_plain_text_doc_opens_percentage_jump() {
    current_case = "plain-text Doc percentage jump";
    std::string source;
    for (int line = 0; line < 320; ++line) {
        source += "Percentage jump line " + std::to_string(line) +
                  " keeps the source offset measurable.\n";
    }
    const std::uint32_t source_size =
        static_cast<std::uint32_t>(source.size());

    nmarkdown::Viewer viewer;
    CHECK(load_plain_text(viewer, std::move(source)));
    CHECK(send(viewer, nmarkdown::InputEventType::OpenMenu));
    CHECK(send_numeric_alias(
        viewer, nmarkdown::InputEventType::PageUp, '7'));
    CHECK(send(viewer, nmarkdown::InputEventType::TextInput, '5'));
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    const std::uint32_t jumped =
        viewer.reader_state(0).position.source_offset;
    CHECK(jumped > source_size * 70U / 100U);
    CHECK(jumped < source_size * 80U / 100U);

    CHECK(send(viewer, nmarkdown::InputEventType::OpenMenu));
    CHECK(send(viewer, nmarkdown::InputEventType::Back));
    CHECK(send_numeric_alias(
        viewer, nmarkdown::InputEventType::PageUp, '7'));
    CHECK(viewer.reader_state(0).position.source_offset < jumped);

    CHECK(send(viewer, nmarkdown::InputEventType::OpenMenu));
    CHECK(send_numeric_alias(
        viewer, nmarkdown::InputEventType::PageDown, '1'));
    CHECK(send(viewer, nmarkdown::InputEventType::TextInput, '0'));
    CHECK(send(viewer, nmarkdown::InputEventType::TextInput, '0'));
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    const std::uint32_t final_page =
        viewer.reader_state(0).position.source_offset;
    CHECK(final_page > source_size * 90U / 100U);
    CHECK(final_page < source_size);
}

void test_plain_text_continuous_scroll_does_not_cancel_itself() {
    current_case = "plain-text continuous drag accumulation";
    std::string source;
    for (int line = 0; line < 96; ++line) {
        source += "Plain text line " + std::to_string(line) +
                  " remains independently scrollable.\n";
    }

    nmarkdown::Viewer viewer;
    CHECK(load_plain_text(viewer, std::move(source)));
    const std::uint32_t start =
        viewer.reader_state(1).position.source_offset;

    // Natural vertical scrolling: an upward finger movement advances.
    CHECK(send(viewer, nmarkdown::InputEventType::PointerScroll, -18));
    const std::uint32_t vertical_forward =
        viewer.reader_state(1).position.source_offset;
    CHECK(vertical_forward > start);
    CHECK(send(viewer, nmarkdown::InputEventType::PointerScroll, 18));
    CHECK(viewer.reader_state(1).position.source_offset == start);

    // Horizontal Scroll uses the other continuous touchpad axis, with the
    // same Natural/Reversed setting and accumulator.
    viewer.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);
    CHECK(send(viewer, nmarkdown::InputEventType::PointerPan, -18));
    CHECK(viewer.reader_state(1).position.source_offset > start);
    CHECK(send(viewer, nmarkdown::InputEventType::PointerPan, 18));
    CHECK(viewer.reader_state(1).position.source_offset == start);
}

void test_deferred_plain_text_page_marks_viewer_dirty() {
    current_case = "deferred plain-text page repaint";
    std::string source;
    for (int line = 0; line < 400; ++line) {
        source += "Deferred page line " + std::to_string(line) +
                  " keeps shaping outside the input event.\n";
    }

    nmarkdown::Viewer viewer;
    CHECK(load_plain_text(viewer, std::move(source)));
    viewer.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);

    bool deferred = false;
    std::uint32_t deferred_from = 0;
    for (int event = 0; event < 3 && !deferred; ++event) {
        viewer.clear_dirty();
        deferred_from =
            viewer.reader_state(0).position.source_offset;
        send(viewer, nmarkdown::InputEventType::PageDown);
        deferred =
            viewer.reader_state(0).position.source_offset ==
            deferred_from;
    }
    CHECK(deferred);
    CHECK(!viewer.dirty());

    ImmediateClock clock;
    int guard = 0;
    while (viewer.reader_state(0).position.source_offset ==
               deferred_from &&
           guard < 2048) {
        CHECK(viewer.perform_incremental_work(clock, 0));
        ++guard;
    }
    CHECK(guard < 2048);
    CHECK(viewer.reader_state(0).position.source_offset >
          deferred_from);
    CHECK(viewer.dirty());
}

void test_plain_text_rewarms_after_glyph_cache_clear() {
    current_case = "plain-text glyph preload invalidation";
    std::string source;
    for (int line = 0; line < 500; ++line) {
        source += "Cache generation line " + std::to_string(line) +
                  " keeps the next page ready without render misses.\n";
    }

    nmarkdown::Viewer viewer;
    CHECK(load_plain_text(viewer, std::move(source)));
    viewer.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);
    std::vector<std::uint16_t> pixels(320U * 240U);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    viewer.render(surface);

    ImmediateClock clock;
    for (int work = 0; work < 4096; ++work) {
        if (!viewer.perform_incremental_work(clock, 0)) break;
    }

    const nmarkdown::ReaderState state = viewer.reader_state(0);
    CHECK(viewer.apply_reader_state(state, 0));
    // Applying reader state clears both document and chrome glyphs. The
    // application repaints the restored current page before accepting the
    // next gesture, so mirror that lifecycle here; the assertion below then
    // isolates readiness of the deferred target page.
    viewer.render(surface);
    const std::uint32_t before =
        viewer.reader_state(0).position.source_offset;
    viewer.clear_dirty();
    send(viewer, nmarkdown::InputEventType::PageDown);
    CHECK(viewer.reader_state(0).position.source_offset == before);
    CHECK(!viewer.dirty());

    int guard = 0;
    while (viewer.reader_state(0).position.source_offset == before &&
           guard < 2048) {
        CHECK(viewer.perform_incremental_work(clock, 0));
        ++guard;
    }
    CHECK(guard < 2048);
    const std::uint64_t misses_before_render =
        viewer.glyph_cache_stats().misses;
    viewer.render(surface);
    CHECK(viewer.glyph_cache_stats().misses ==
          misses_before_render);
}

void test_render_sharpness_setting_is_key_only_and_persisted() {
    current_case = "render sharpness is key-only and persisted";
    nmarkdown::Viewer viewer;
    CHECK(viewer.render_sharpness() == nmarkdown::kDefaultRenderSharpness);
    CHECK(open_settings(viewer));
    for (int row = 0; row < 7; ++row) {
        CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
    }
    CHECK(!tap(viewer));
    CHECK(viewer.render_sharpness() == nmarkdown::kDefaultRenderSharpness);
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(viewer.render_sharpness() ==
          nmarkdown::kDefaultRenderSharpness + 1);
    CHECK(send(viewer, nmarkdown::InputEventType::PanLeft));
    CHECK(viewer.render_sharpness() == nmarkdown::kDefaultRenderSharpness);
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(viewer.render_sharpness() ==
          nmarkdown::kDefaultRenderSharpness + 1);
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    CHECK(viewer.render_sharpness() ==
          nmarkdown::kDefaultRenderSharpness + 1);
    CHECK(viewer.reader_state(0).render_sharpness ==
          nmarkdown::kDefaultRenderSharpness + 1);
}

// The "Font preload" settings row turns resident-font promotion off
// entirely. Committing a change requests one global preference save; an
// untouched session requests none.
// Single-step navigation in the modal lists wraps at both ends: stepping up
// from the first row lands on the last and stepping down from the last
// returns to the first. Each wrap is proven by activating the row it lands
// on rather than by peeking at selection state.
// Opening a CJK document with no CJK-role font asks the user to pick one:
// Enter on the prompt requests the font manager, Esc continues without and
// the passive once-per-session behavior is retained.
void test_missing_cjk_font_prompt_offers_font_manager() {
    current_case = "missing CJK font prompt";
    {
        nmarkdown::Viewer viewer;
        CHECK(load_markdown(viewer, u8"中文内容需要字体支持。\n"));
        CHECK(!viewer.take_font_menu_request());
        CHECK(send(viewer, nmarkdown::InputEventType::Activate));
        CHECK(viewer.take_font_menu_request());
        CHECK(!viewer.take_font_menu_request());
    }
    {
        nmarkdown::Viewer viewer;
        CHECK(load_markdown(viewer, u8"中文内容需要字体支持。\n"));
        CHECK(send(viewer, nmarkdown::InputEventType::Back));
        CHECK(!viewer.take_font_menu_request());
        CHECK(!viewer.quit_requested());
        // A later ordinary message keeps plain Enter-closes semantics.
        viewer.show_message("Note", "Ordinary message");
        CHECK(send(viewer, nmarkdown::InputEventType::Activate));
        CHECK(!viewer.take_font_menu_request());
    }
    {
        // Documents without CJK content never prompt: an immediate Activate
        // finds no armed dialog and requests nothing.
        nmarkdown::Viewer viewer;
        CHECK(load_markdown(viewer, "Latin only body.\n"));
        send(viewer, nmarkdown::InputEventType::Activate);
        CHECK(!viewer.take_font_menu_request());
        CHECK(!viewer.quit_requested());
    }
}

void test_system_lists_wrap_at_both_ends() {
    current_case = "system lists wrap at both ends";

    // Settings: Up from the first row lands on the final Fonts action row.
    {
        nmarkdown::Viewer viewer;
        CHECK(load_markdown(viewer, "Plain body.\n"));
        CHECK(open_settings(viewer));
        CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineUp));
        CHECK(send(viewer, nmarkdown::InputEventType::Activate));
        CHECK(viewer.take_font_menu_request());
    }

    // Settings: Down from the last row wraps back to the Theme row.
    {
        nmarkdown::Viewer viewer;
        CHECK(load_markdown(viewer, "Plain body.\n"));
        CHECK(open_settings(viewer));
        CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineUp));
        CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
        CHECK(!viewer.dark_theme());
        CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
        CHECK(viewer.dark_theme());
        CHECK(send(viewer, nmarkdown::InputEventType::Back));
    }

    // Table of contents: Up from the first heading selects the last one.
    // The document is taller than one viewport so the jump is observable.
    {
        nmarkdown::Viewer viewer;
        std::string source = "# First\n\n";
        for (int index = 0; index < 30; ++index) {
            source += "Padding keeps the second heading far below the "
                      "first viewport.\n\n";
        }
        source += "## Second\n\nMore.\n";
        CHECK(load_markdown(viewer, source));
        CHECK(send(viewer, nmarkdown::InputEventType::OpenMenu));
        CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineUp));
        CHECK(send(viewer, nmarkdown::InputEventType::Activate));
        CHECK(viewer.scroll_y() > 0);
    }

    // Document browser: Up from the first entry selects the last document.
    {
        nmarkdown::Viewer viewer;
        const std::vector<std::string> paths = {
            "/documents/a.md", "/documents/b.md", "/documents/c.md"};
        viewer.show_document_browser(paths, false);
        CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineUp));
        CHECK(send(viewer, nmarkdown::InputEventType::Activate));
        std::string target;
        CHECK(viewer.take_document_open_request(target));
        CHECK(target == "/documents/c.md");
    }
}

void test_font_preload_setting_toggles_and_requests_save() {
    current_case = "font preload setting";
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, "Plain paragraph body.\n"));
    CHECK(viewer.resident_font_preload());
    CHECK(!viewer.take_font_preload_save_request());

    CHECK(open_settings(viewer));
    for (int row = 0; row < 11; ++row) {
        CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
    }
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(!viewer.resident_font_preload());
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    CHECK(viewer.take_font_preload_save_request());
    CHECK(!viewer.take_font_preload_save_request());

    // The selected row is retained, so reopening edits the same switch.
    CHECK(open_settings(viewer));
    CHECK(send(viewer, nmarkdown::InputEventType::PanLeft));
    CHECK(viewer.resident_font_preload());
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    CHECK(viewer.take_font_preload_save_request());

    // A session that leaves the switch untouched requests no save.
    CHECK(open_settings(viewer));
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    CHECK(!viewer.take_font_preload_save_request());
}

void test_wide_content_pan_keeps_conventional_directions() {
    current_case = "conventional wide-content pan";
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, long_document_without_headings()));
    CHECK(viewer.reader_state(0).code_wrap);
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    CHECK(viewer.pan_x() == 0);

    constexpr int kWidth = 320;
    constexpr int kHeight = 240;
    std::vector<std::uint16_t> pixels(kWidth * kHeight, 0);
    nmarkdown::Surface565 surface(pixels.data(), kWidth, kHeight, kWidth);
    viewer.render(surface);
    const std::vector<std::uint16_t> focused_origin = pixels;

    CHECK(!send(viewer, nmarkdown::InputEventType::PanLeft));
    CHECK(viewer.pan_x() == 0);
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(viewer.pan_x() > 0);
    CHECK(viewer.reader_state(0).code_wrap);
    std::fill(pixels.begin(), pixels.end(), 0);
    viewer.render(surface);
    CHECK(pixels != focused_origin);
    CHECK(send(viewer, nmarkdown::InputEventType::PanLeft));
    CHECK(viewer.pan_x() == 0);
    CHECK(viewer.reader_state(0).code_wrap);
}

// When the viewport holds content that requires horizontal panning, left and
// right input engages the pan directly — no prior Enter needed — and page
// navigation never fires, including at the pan limits and immediately after
// an explicit focus dismissal.
void test_wide_block_reserves_horizontal_input_for_pan() {
    current_case = "wide block reserves horizontal input";
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, long_document_without_headings()));
    CHECK(viewer.scroll_y() == 0);
    CHECK(viewer.pan_x() == 0);

    // PanRight pans the wide code block instead of turning a page.
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(viewer.pan_x() > 0);
    CHECK(viewer.scroll_y() == 0);

    // Return to the pan origin; further PanLeft saturates without paging.
    CHECK(send(viewer, nmarkdown::InputEventType::PanLeft));
    CHECK(viewer.pan_x() == 0);
    CHECK(!send(viewer, nmarkdown::InputEventType::PanLeft));
    CHECK(viewer.pan_x() == 0);
    CHECK(viewer.scroll_y() == 0);

    // Dismissing focus with Back does not hand the next press to paging;
    // horizontal input re-engages the pan while the block is in view.
    CHECK(send(viewer, nmarkdown::InputEventType::Back));
    CHECK(!viewer.quit_requested());
    CHECK(viewer.pan_x() == 0);
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(viewer.pan_x() > 0);
    CHECK(viewer.scroll_y() == 0);

    // Horizontal swipes are reserved the same way in vertical mode.
    CHECK(send(viewer, nmarkdown::InputEventType::Back));
    CHECK(send(viewer, nmarkdown::InputEventType::SwipeLeft));
    CHECK(viewer.pan_x() > 0);
    CHECK(viewer.scroll_y() == 0);
    CHECK(send(viewer, nmarkdown::InputEventType::SwipeRight));
    CHECK(viewer.pan_x() == 0);
    CHECK(viewer.scroll_y() == 0);

    // Scrolling past the wide block releases the reservation: left/right
    // page again over plain prose.
    CHECK(send(viewer, nmarkdown::InputEventType::Back));
    CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
    const int paged_scroll = viewer.scroll_y();
    CHECK(paged_scroll > 0);
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(viewer.pan_x() == 0);
    CHECK(viewer.scroll_y() > paged_scroll);
    CHECK(send(viewer, nmarkdown::InputEventType::PanLeft));
    CHECK(viewer.scroll_y() == paged_scroll);
    CHECK(viewer.pan_x() == 0);
}

void test_plain_document_activation_is_inert() {
    current_case = "plain document activation is inert";
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(
        viewer,
        "A plain paragraph has no link, list item, or oversized block target.\n"));
    viewer.clear_dirty();
    CHECK(!viewer.take_state_save_request());
    const DocumentSnapshot before = snapshot(viewer);

    CHECK(!send(viewer, nmarkdown::InputEventType::Activate));
    expect_same_document_state(before, viewer);
    CHECK(!viewer.dirty());
    CHECK(!viewer.take_state_save_request());
    CHECK(!viewer.quit_requested());
}

void test_wrapped_code_focus_snaps_to_canvas_and_restores_scroll() {
    current_case = "wrapped code focus restores deep scroll";
    const std::string source = "```text\n" + std::string(900, 'W') +
                               "\n```\n\nFollowing prose.\n";
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    CHECK(viewer.reader_state(0).code_wrap);

    constexpr int kWidth = 320;
    constexpr int kHeight = 240;
    std::vector<std::uint16_t> pixels(kWidth * kHeight, 0);
    nmarkdown::Surface565 surface(pixels.data(), kWidth, kHeight, kWidth);
    viewer.render(surface);
    for (int step = 0; step < 8; ++step) {
        CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
    }
    const int wrapped_scroll = viewer.scroll_y();
    CHECK(wrapped_scroll > 0);

    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    CHECK(viewer.scroll_y() == 0);
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(viewer.pan_x() > 0);
    CHECK(send(viewer, nmarkdown::InputEventType::Back));
    CHECK(viewer.pan_x() == 0);
    CHECK(viewer.scroll_y() == wrapped_scroll);
    CHECK(!viewer.quit_requested());
}

void test_large_display_formula_can_be_focused_and_panned() {
    current_case = "large display formula focus and pan";
    std::string source = R"(A short prose block deliberately remains at the viewport top.

$$
\begin{align}
    v + w & = 0  & \text{Given} \tag 1 \\
       -w & = -w + 0 & \text{additive identity} \tag 2 \\
   -w + 0 & = -w + (v + w) & \text{equations $(1)$ and $(2)$} \\
\end{align}
$$
)";
    for (int index = 0; index < 36; ++index) {
        source += "Filler keeps screen-step navigation observable after focus closes.\n\n";
    }

    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    CHECK(!viewer.dark_theme());
    CHECK(viewer.pan_x() == 0);
    CHECK(viewer.scroll_y() == 0);

    constexpr int kWidth = 320;
    constexpr int kHeight = 240;
    std::vector<std::uint16_t> pixels(kWidth * kHeight, 0);
    nmarkdown::Surface565 surface(pixels.data(), kWidth, kHeight, kWidth);
    const std::uint16_t accent = nmarkdown::rgb565(61, 144, 214);
    const auto has_marker = [&](int x) {
        int consecutive = 0;
        for (int y = 18; y < kHeight; ++y) {
            const bool pair = pixels[static_cast<std::size_t>(y) * kWidth + x] == accent &&
                              pixels[static_cast<std::size_t>(y) * kWidth + x + 1] == accent;
            consecutive = pair ? consecutive + 1 : 0;
            if (consecutive >= 3) return true;
        }
        return false;
    };

    viewer.render(surface);
    CHECK(has_marker(313));
    CHECK(!has_marker(5));

    // Enter chooses the visible equation even though the prose block is still
    // the document's top unit. It must focus the equation instead of behaving
    // like an inert document-level activation.
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    CHECK(!viewer.dark_theme());
    CHECK(send(viewer, nmarkdown::InputEventType::PanRight));
    CHECK(viewer.pan_x() == 12);
    std::fill(pixels.begin(), pixels.end(), 0);
    viewer.render(surface);
    CHECK(has_marker(5));
    CHECK(has_marker(313));

    // Full horizontal touchpad swipes pan the focused canvas by a viewport
    // without moving the document underneath.
    const int scroll_before_swipe = viewer.scroll_y();
    const int page_before_swipe = viewer.current_page();
    CHECK(send(viewer, nmarkdown::InputEventType::SwipeLeft));
    CHECK(viewer.pan_x() > 12);
    CHECK(viewer.scroll_y() == scroll_before_swipe);
    CHECK(viewer.current_page() == page_before_swipe);
    CHECK(send(viewer, nmarkdown::InputEventType::SwipeRight));
    CHECK(viewer.pan_x() == 0);

    // Page keys stay vertical even while the formula owns horizontal focus.
    const int vertical_start = viewer.scroll_y();
    CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
    CHECK(viewer.scroll_y() > vertical_start);
    CHECK(viewer.pan_x() == 0);
    CHECK(send(viewer, nmarkdown::InputEventType::PageUp));
    CHECK(viewer.scroll_y() == vertical_start);
    CHECK(viewer.pan_x() == 0);

    // Focus owns continuous horizontal drag in Horizontal Scroll mode, but
    // screen-step keys still move the document rather than changing local pan.
    viewer.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);
    const int focused_scroll = viewer.scroll_y();
    CHECK(send(viewer, nmarkdown::InputEventType::PointerPan, -40));
    CHECK(viewer.pan_x() > 0);
    CHECK(viewer.scroll_y() == focused_scroll);
    CHECK(send(viewer, nmarkdown::InputEventType::PointerPan, 40));
    CHECK(viewer.pan_x() == 0);
    CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
    CHECK(viewer.pan_x() == 0);
    CHECK(viewer.scroll_y() > focused_scroll);
    CHECK(send(viewer, nmarkdown::InputEventType::PageUp));
    CHECK(viewer.pan_x() == 0);
    CHECK(viewer.scroll_y() == focused_scroll);
    CHECK(send(viewer, nmarkdown::InputEventType::SwipeDown));
    CHECK(viewer.pan_x() == 0);
    CHECK(viewer.scroll_y() > focused_scroll);
    CHECK(send(viewer, nmarkdown::InputEventType::SwipeUp));
    CHECK(viewer.scroll_y() == focused_scroll);

    // The right-aligned annotation and tag lane lives at the formula's far
    // edge. Pan all the way there and verify the overflow affordance changes
    // from "more on the right" to "more on the left". This couples the
    // alignment-coordinate regression in test_math_layout to the actual
    // 320-pixel focused viewport used for the review screenshot.
    int fine_pan_steps = 0;
    while (fine_pan_steps < 256 &&
           send(viewer, nmarkdown::InputEventType::PanRight)) {
        ++fine_pan_steps;
    }
    CHECK(fine_pan_steps > 0);
    CHECK(viewer.pan_x() > 0);
    std::fill(pixels.begin(), pixels.end(), 0);
    viewer.render(surface);
    CHECK(has_marker(5));
    CHECK(!has_marker(313));

    // Esc leaves the focus layer and returns to the unchanged document.
    CHECK(send(viewer, nmarkdown::InputEventType::Back));
    CHECK(viewer.pan_x() == 0);
    CHECK(!viewer.quit_requested());
    CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
    CHECK(viewer.scroll_y() > focused_scroll);
}

void test_formula_pan_is_not_artificially_capped() {
    current_case = "formula pan reaches measured width beyond 2048 pixels";
    const std::string source = "$$\n\\text{" + std::string(800, 'W') + "}\n$$\n";
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));

    int successful_pages = 0;
    while (successful_pages < 128 &&
           send(viewer, nmarkdown::InputEventType::SwipeLeft)) {
        ++successful_pages;
    }
    CHECK(successful_pages > 0);
    CHECK(viewer.pan_x() > 2048);
    CHECK(!send(viewer, nmarkdown::InputEventType::SwipeLeft));
    CHECK(send(viewer, nmarkdown::InputEventType::Back));
    CHECK(viewer.pan_x() == 0);
    CHECK(!viewer.quit_requested());
}

void test_tall_formula_pages_continue_into_following_content() {
    current_case = "tall formula can be paged through and past";
    std::string source = "Before the oversized display.\n\n$$\n\\begin{matrix}\n";
    for (int row = 0; row < 28; ++row) {
        source += "\\frac{x_{" + std::to_string(row) +
                  "}^2+1}{\\sqrt{y_{" + std::to_string(row) +
                  "}+1}} & = & " + std::to_string(row) + " \\\\\n";
    }
    source += "\\end{matrix}\n$$\n\n## After formula\n\n"
              "This trailing paragraph must remain reachable.\n";

    auto document = std::make_unique<nmarkdown::MarkdownDocument>();
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        *document, error));
    const nmarkdown::NodeId trailing_node = document->ir.blocks.empty()
                                                ? nmarkdown::kInvalidNode
                                                : static_cast<nmarkdown::NodeId>(
                                                      document->ir.blocks.size() - 1);
    nmarkdown::Viewer viewer;
    nmarkdown::DocumentProbe probe;
    probe.size = source.size();
    CHECK(viewer.set_markdown_document(std::move(document), probe, error));

    constexpr int kWidth = 320;
    constexpr int kHeight = 240;
    std::vector<std::uint16_t> pixels(kWidth * kHeight, 0);
    nmarkdown::Surface565 surface(pixels.data(), kWidth, kHeight, kWidth);
    viewer.render(surface);  // Measure the oversized formula before paging.
    viewer.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);
    CHECK(viewer.total_pages() > 3);

    int steps = 0;
    while (viewer.scroll_y() < viewer.max_scroll_y() && steps < 64) {
        const int before = viewer.scroll_y();
        CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
        CHECK(viewer.scroll_y() > before);
        viewer.render(surface);
        ++steps;
    }
    CHECK(steps > 2);
    CHECK(steps < 64);
    CHECK(viewer.current_page() == viewer.total_pages());
    CHECK(viewer.scroll_y() == viewer.max_scroll_y());
    CHECK(viewer.reader_state(0).position.nearest_block == trailing_node);
}

void test_line_down_aligns_the_last_visible_markdown_line() {
    current_case = "text-only line movement aligns its destination edge";
    std::string source = "```text\n";
    for (int line = 0; line < 80; ++line) {
        source += "line " + std::to_string(line) + " alignment probe\n";
    }
    source += "```\n";

    nmarkdown::MarkdownDocument reference_document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        reference_document, error));
    nmarkdown::TextSystem reference_text;
    CHECK(reference_text.initialize(error));
    nmarkdown::VirtualDocumentLayout reference_layout;
    nmarkdown::LayoutSignature signature;
    signature.content_width = 310;
    CHECK(reference_layout.initialize(reference_document, reference_text,
                                      signature, error));
    const nmarkdown::BlockLayout* block = reference_layout.layout_unit(0);
    CHECK(block != nullptr);
    if (block == nullptr) return;

    struct LineBounds {
        int top = 0;
        int bottom = 0;
    };
    std::vector<LineBounds> lines;
    constexpr int kViewportHeight = 220;
    for (const nmarkdown::LayoutLine& line : block->lines) {
        const int top = nmarkdown::fx_floor(reference_layout.unit_top(0)) +
                        nmarkdown::fx_floor(line.baseline_y) -
                        nmarkdown::fx_ceil(line.ascent);
        const nmarkdown::Fx descent = line.descent < 0
                                           ? -line.descent : line.descent;
        const int bottom = nmarkdown::fx_floor(reference_layout.unit_top(0)) +
                           nmarkdown::fx_floor(line.baseline_y) +
                           nmarkdown::fx_ceil(descent);
        lines.push_back({top, std::max(top + 1, bottom)});
    }

    int start = -1;
    int first_target = -1;
    int second_target = -1;
    int first_bottom = -1;
    int second_bottom = -1;
    for (int candidate = 1; candidate < kViewportHeight && start < 0;
         ++candidate) {
        const int boundary = candidate + kViewportHeight;
        for (std::size_t index = 0; index + 1 < lines.size(); ++index) {
            const LineBounds& line = lines[index];
            if (line.top >= boundary || line.bottom <= boundary) continue;
            const int target = line.bottom - kViewportHeight;
            const int next_boundary = target + kViewportHeight;
            for (std::size_t next = index + 1; next < lines.size(); ++next) {
                if (lines[next].bottom <= next_boundary) continue;
                const int following = lines[next].bottom - kViewportHeight;
                if (target > candidate && following > target &&
                    target - candidate != 18) {
                    start = candidate;
                    first_target = target;
                    second_target = following;
                    first_bottom = line.bottom;
                    second_bottom = lines[next].bottom;
                }
                break;
            }
            break;
        }
    }
    CHECK(start > 0);
    CHECK(first_target > start);
    CHECK(second_target > first_target);
    if (start <= 0 || first_target <= start || second_target <= first_target) {
        return;
    }

    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    viewer.render(surface);
    CHECK(send(viewer, nmarkdown::InputEventType::PointerScroll, -start));
    CHECK(viewer.scroll_y() == start);

    CHECK(send_numeric_alias(viewer,
                             nmarkdown::InputEventType::ScrollLineDown, '2'));
    CHECK(viewer.scroll_y() == first_target);
    CHECK(viewer.scroll_y() + kViewportHeight ==
          first_bottom);

    CHECK(send_numeric_alias(viewer,
                             nmarkdown::InputEventType::ScrollLineDown, '2'));
    CHECK(viewer.scroll_y() == second_target);
    CHECK(viewer.scroll_y() + kViewportHeight == second_bottom);

    const auto is_line_top = [&lines](int position) {
        for (const LineBounds& line : lines) {
            if (line.top == position) return true;
        }
        return position == 0;
    };
    const int before_first_up = viewer.scroll_y();
    CHECK(send_numeric_alias(viewer,
                             nmarkdown::InputEventType::ScrollLineUp, '8'));
    CHECK(viewer.scroll_y() < before_first_up);
    CHECK(is_line_top(viewer.scroll_y()));
    const int before_second_up = viewer.scroll_y();
    CHECK(send_numeric_alias(viewer,
                             nmarkdown::InputEventType::ScrollLineUp, '8'));
    CHECK(viewer.scroll_y() < before_second_up);
    CHECK(is_line_top(viewer.scroll_y()));

    // Any formula in the current viewport disables text-line alignment. This
    // keeps large or irregular math content on the bounded legacy step.
    std::string formula_source =
        "Text with inline math $\\frac{x^2+1}{y}$ on the first screen.\n\n";
    for (int index = 0; index < 48; ++index) {
        formula_source += "Following text keeps the document scrollable.\n\n";
    }
    nmarkdown::Viewer formula_viewer;
    CHECK(load_markdown(formula_viewer, formula_source));
    formula_viewer.render(surface);
    CHECK(send_numeric_alias(formula_viewer,
                             nmarkdown::InputEventType::ScrollLineDown, '2'));
    CHECK(formula_viewer.scroll_y() == 18);
    CHECK(send_numeric_alias(formula_viewer,
                             nmarkdown::InputEventType::ScrollLineDown, '2'));
    CHECK(formula_viewer.scroll_y() == 36);
    CHECK(send_numeric_alias(formula_viewer,
                             nmarkdown::InputEventType::ScrollLineUp, '8'));
    CHECK(formula_viewer.scroll_y() == 18);
}

void test_page_keys_land_on_complete_line_boundaries() {
    current_case = "screen steps skip fully displayed boundary lines";
    std::string source = "```text\n";
    for (int line = 0; line < 80; ++line) {
        source += "line " + std::to_string(line) + " alignment probe\n";
    }
    source += "```\n";

    nmarkdown::MarkdownDocument reference_document;
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        reference_document, error));
    nmarkdown::TextSystem reference_text;
    CHECK(reference_text.initialize(error));
    nmarkdown::VirtualDocumentLayout reference_layout;
    nmarkdown::LayoutSignature signature;
    signature.content_width = 310;
    CHECK(reference_layout.initialize(reference_document, reference_text,
                                      signature, error));
    const nmarkdown::BlockLayout* block = reference_layout.layout_unit(0);
    CHECK(block != nullptr);
    if (block == nullptr) return;

    struct LineBounds {
        int top = 0;
        int bottom = 0;
    };
    std::vector<LineBounds> lines;
    constexpr int kViewportHeight = 220;
    for (const nmarkdown::LayoutLine& line : block->lines) {
        const int top = nmarkdown::fx_floor(reference_layout.unit_top(0)) +
                        nmarkdown::fx_floor(line.baseline_y) -
                        nmarkdown::fx_ceil(line.ascent);
        const nmarkdown::Fx descent = line.descent < 0
                                           ? -line.descent : line.descent;
        const int bottom = nmarkdown::fx_floor(reference_layout.unit_top(0)) +
                           nmarkdown::fx_floor(line.baseline_y) +
                           nmarkdown::fx_ceil(descent);
        lines.push_back({top, std::max(top + 1, bottom)});
    }
    const int reference_max_scroll = std::max(
        0, nmarkdown::fx_ceil(reference_layout.total_height()) -
               kViewportHeight);

    int expected = -1;
    for (const LineBounds& line : lines) {
        if (line.top < kViewportHeight &&
            line.bottom > kViewportHeight) {
            // Only a line clipped by the old viewport remains as context.
            expected = line.top;
            break;
        }
        if (line.top >= kViewportHeight) {
            // A fully displayed bottom line must not be repeated.
            expected = line.top;
            break;
        }
    }
    CHECK(expected >= 0);
    if (expected < 0) return;
    CHECK(expected > 0);
    CHECK(expected < reference_max_scroll);
    CHECK(expected >= kViewportHeight / 2);

    const auto exercise_page_down = [&](nmarkdown::ReadingMode mode,
                                        nmarkdown::InputEventType forward_swipe,
                                        nmarkdown::InputEventType reverse_swipe) {
        nmarkdown::Viewer viewer;
        CHECK(load_markdown(viewer, source));
        viewer.set_reading_mode(mode);
        std::vector<std::uint16_t> pixels(320 * 240, 0);
        nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
        viewer.render(surface);
        CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
        CHECK(viewer.scroll_y() == expected);
        CHECK(send(viewer, nmarkdown::InputEventType::PageUp));
        CHECK(viewer.scroll_y() == 0);

        CHECK(send(viewer, forward_swipe));
        CHECK(viewer.scroll_y() == expected);
        CHECK(send(viewer, reverse_swipe));
        CHECK(viewer.scroll_y() == 0);
    };
    exercise_page_down(nmarkdown::ReadingMode::VerticalScroll,
                       nmarkdown::InputEventType::SwipeRight,
                       nmarkdown::InputEventType::SwipeLeft);
    exercise_page_down(nmarkdown::ReadingMode::HorizontalScroll,
                       nmarkdown::InputEventType::SwipeDown,
                       nmarkdown::InputEventType::SwipeUp);

    int mostly_visible_start = -1;
    int mostly_visible_target = -1;
    for (int start = 1; start < kViewportHeight &&
                        mostly_visible_start < 0; ++start) {
        const int boundary = start + kViewportHeight;
        for (std::size_t index = 0; index + 1 < lines.size(); ++index) {
            const LineBounds& line = lines[index];
            if (line.top >= boundary || line.bottom <= boundary) continue;
            const int height = line.bottom - line.top;
            const int visible = boundary - line.top;
            if (visible * 100 >= height * 85) {
                mostly_visible_start = start;
                mostly_visible_target = lines[index + 1].top;
                break;
            }
        }
    }
    CHECK(mostly_visible_start > 0);
    CHECK(mostly_visible_target > mostly_visible_start + kViewportHeight);
    if (mostly_visible_start > 0 && mostly_visible_target > 0) {
        nmarkdown::Viewer viewer;
        CHECK(load_markdown(viewer, source));
        std::vector<std::uint16_t> pixels(320 * 240, 0);
        nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
        viewer.render(surface);
        CHECK(send(viewer, nmarkdown::InputEventType::PointerScroll,
                   -mostly_visible_start));
        CHECK(viewer.scroll_y() == mostly_visible_start);
        CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
        CHECK(viewer.scroll_y() == mostly_visible_target);
        int expected_page_up = 0;
        for (const LineBounds& line : lines) {
            if (line.top <= mostly_visible_start) {
                expected_page_up = std::max(expected_page_up, line.top);
            }
        }
        CHECK(send(viewer, nmarkdown::InputEventType::PageUp));
        CHECK(viewer.scroll_y() == expected_page_up);
    }

    // Page Down may produce a clipped top only at a bottom-aligned final or
    // overlap page. In that case the clipped row must intersect the complete
    // preceding viewport; wholly new rows always start at their top.
    {
        nmarkdown::Viewer viewer;
        CHECK(load_markdown(viewer, source));
        std::vector<std::uint16_t> pixels(320 * 240, 0);
        nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
        viewer.render(surface);
        int guard = 0;
        while (viewer.scroll_y() < viewer.max_scroll_y() && guard < 64) {
            const int previous_top = viewer.scroll_y();
            CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
            const int next_top = viewer.scroll_y();
            CHECK(next_top > previous_top);
            for (const LineBounds& line : lines) {
                if (line.top < next_top && line.bottom > next_top) {
                    CHECK(line.bottom > previous_top);
                    CHECK(line.top < previous_top + kViewportHeight);
                    break;
                }
            }
            viewer.render(surface);
            ++guard;
        }
        CHECK(guard < 64);
        CHECK(viewer.scroll_y() == viewer.max_scroll_y());
    }
}

void test_render_culls_lines_outside_the_viewport() {
    current_case = "off-screen shaped-line culling";
    std::string source;
    for (int line = 0; line < 128; ++line) {
        source += "Culling line " + std::to_string(line) +
                  " has enough visible glyphs for a useful bound.  \n";
    }
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    const nmarkdown::GlyphCacheStats before = viewer.glyph_cache_stats();
    viewer.render(surface);
    const nmarkdown::GlyphCacheStats after = viewer.glyph_cache_stats();
    const std::uint64_t accesses = (after.hits - before.hits) +
                                   (after.misses - before.misses);
    CHECK(accesses > 0);
    // The viewport contains roughly a dozen lines. The old painter submitted
    // all 128 lines (>5,000 glyph probes) before clipping their pixels.
    CHECK(accesses < 1000);
}

void test_font_manager_suggests_shared_multifunction_roles() {
    current_case = "font manager shared-role suggestions";
    nmarkdown::FontFaceCatalogEntry sarasa;
    sarasa.path = "/documents/SarasaFixedSC-Regular.ttf.tns";
    sarasa.family = "Sarasa Fixed SC";
    sarasa.has_latin = true;
    sarasa.has_cjk = true;
    sarasa.fixed_pitch = true;
    nmarkdown::Viewer viewer;
    viewer.show_font_manager({sarasa}, {});
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    CHECK(send(viewer, nmarkdown::InputEventType::PageDown));
    CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
    CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    CHECK(send(viewer, nmarkdown::InputEventType::Back));
    CHECK(send(viewer, nmarkdown::InputEventType::ScrollLineDown));
    CHECK(send(viewer, nmarkdown::InputEventType::Activate));
    std::array<std::string, nmarkdown::kExternalFontRoleCount> roles;
    CHECK(viewer.take_font_assignments(roles));
    CHECK(roles[0].empty());
    CHECK(roles[2] == sarasa.path);
    CHECK(roles[3] == sarasa.path);
}

}  // namespace

// render() paints the full frame from three regions (header fill, the
// render_document branch's viewport fill, and the bottom inset strip)
// instead of a redundant whole-surface clear. Prove the union still covers
// every pixel in every document state and theme: prefill with a sentinel
// color no theme uses and require that none of it survives a render.
void test_full_repaint_covers_every_pixel() {
    current_case = "full repaint coverage";
    constexpr int kWidth = 320;
    constexpr int kHeight = 240;
    constexpr std::uint16_t kSentinel = 0xF81F;  // saturated magenta

    struct Scenario {
        const char* name;
        std::function<bool(nmarkdown::Viewer&)> setup;
    };
    const Scenario scenarios[] = {
        {"no document", [](nmarkdown::Viewer&) { return true; }},
        {"markdown",
         [](nmarkdown::Viewer& viewer) {
             return load_markdown(viewer,
                                  "# Title\n\nBody paragraph\n\n- item\n");
         }},
        {"plain text",
         [](nmarkdown::Viewer& viewer) {
             return load_plain_text(viewer, "line one\nline two\n");
         }},
        {"document error",
         [](nmarkdown::Viewer& viewer) {
             viewer.set_document_error("unreadable input");
             return true;
         }},
    };

    // Coverage is a property of fill geometry alone; themes only swap the
    // fill colors, so one theme proves the union for all of them.
    for (const Scenario& scenario : scenarios) {
        nmarkdown::Viewer viewer;
        CHECK(scenario.setup(viewer));
        std::vector<std::uint16_t> pixels(
            static_cast<std::size_t>(kWidth) * kHeight, kSentinel);
        nmarkdown::Surface565 surface(pixels.data(), kWidth, kHeight,
                                      kWidth);
        viewer.render(surface);
        std::size_t uncovered = 0;
        for (std::uint16_t pixel : pixels) {
            if (pixel == kSentinel) ++uncovered;
        }
        if (uncovered != 0) {
            std::fprintf(stderr, "coverage gap: %zu pixels in '%s'\n",
                         uncovered, scenario.name);
        }
        CHECK(uncovered == 0);
    }
}

int main() {
    test_quit_has_global_priority();
    test_back_closes_visible_layer_before_reader();
    test_touchpad_modes_and_gesture_direction();
    test_plain_text_doc_opens_percentage_jump();
    test_plain_text_continuous_scroll_does_not_cancel_itself();
    test_deferred_plain_text_page_marks_viewer_dirty();
    test_plain_text_rewarms_after_glyph_cache_clear();
    test_render_sharpness_setting_is_key_only_and_persisted();
    test_missing_cjk_font_prompt_offers_font_manager();
    test_system_lists_wrap_at_both_ends();
    test_font_preload_setting_toggles_and_requests_save();
    test_wide_content_pan_keeps_conventional_directions();
    test_wide_block_reserves_horizontal_input_for_pan();
    test_plain_document_activation_is_inert();
    test_wrapped_code_focus_snaps_to_canvas_and_restores_scroll();
    test_large_display_formula_can_be_focused_and_panned();
    test_formula_pan_is_not_artificially_capped();
    test_tall_formula_pages_continue_into_following_content();
    test_line_down_aligns_the_last_visible_markdown_line();
    test_page_keys_land_on_complete_line_boundaries();
    test_render_culls_lines_outside_the_viewport();
    test_font_manager_suggests_shared_multifunction_roles();
    test_full_repaint_covers_every_pixel();
    test_passive_overlay_isolates_both_touchpad_modes();
    test_non_owning_shortcuts_do_not_leak_through_overlay_matrix();
    test_search_treats_navigation_digits_as_query_text();
    test_modal_swipes_and_touchpad_activation();

    if (failures != 0) {
        std::fprintf(stderr, "%d viewer interaction test(s) failed\n", failures);
        return 1;
    }
    std::puts("viewer interaction tests passed");
    return 0;
}
