#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#include "nmarkdown/app/application.h"
#include "nmarkdown/app/viewer.h"
#include "nmarkdown/document/markdown.h"
#include "nmarkdown/document/state.h"
#include "nmarkdown/generated/core_font_pack.h"
#include "nmarkdown/platform/platform.h"
#include "nmarkdown/render/primitives.h"
#include "nmarkdown/render/surface565.h"
#include "nmarkdown/text/compositor.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::fprintf(stderr,                                               \
                         "CHECK failed at %s:%d: %s\n",                      \
                         __FILE__,                                             \
                         __LINE__,                                             \
                         #condition);                                          \
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

bool select_code_block_pan_mode(nmarkdown::Viewer& viewer) {
    if (!viewer.reader_state(0).code_wrap) return true;
    if (!viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0})) {
        return false;
    }
    for (int row = 0; row < 5; ++row) {
        if (!viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0})) {
            return false;
        }
    }
    if (!viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}) ||
        !viewer.handle_event({nmarkdown::InputEventType::Activate, 0})) {
        return false;
    }
    return !viewer.reader_state(0).code_wrap;
}

int leading_color_width(const nmarkdown::Surface565& surface,
                        std::uint16_t color) {
    int width = 0;
    while (width < surface.width() && surface.pixel(width, 0) == color) {
        ++width;
    }
    return width;
}

void test_surface_and_primitives() {
    std::vector<std::uint16_t> pixels(8 * 6, 0);
    nmarkdown::Surface565 surface(pixels.data(), 8, 6, 8);
    CHECK(surface.valid());
    CHECK(nmarkdown::rgb565(255, 0, 0) == 0xF800U);
    CHECK(nmarkdown::rgb565(0, 255, 0) == 0x07E0U);
    CHECK(nmarkdown::rgb565(0, 0, 255) == 0x001FU);

    surface.clear(nmarkdown::rgb565(1, 2, 3));
    nmarkdown::fill_rect(surface,
                         {-2, 1, 6, 4},
                         nmarkdown::rgb565(255, 0, 0),
                         {1, 2, 4, 2});
    CHECK(surface.pixel(0, 2) != nmarkdown::rgb565(255, 0, 0));
    CHECK(surface.pixel(1, 2) == nmarkdown::rgb565(255, 0, 0));
    CHECK(surface.pixel(3, 3) == nmarkdown::rgb565(255, 0, 0));
    CHECK(surface.pixel(4, 3) != nmarkdown::rgb565(255, 0, 0));

    nmarkdown::draw_line(surface, 0, 0, 7, 5, nmarkdown::rgb565(0, 0, 255));
    CHECK(surface.pixel(0, 0) == nmarkdown::rgb565(0, 0, 255));
    CHECK(surface.pixel(7, 5) == nmarkdown::rgb565(0, 0, 255));
}

void test_file_probe() {
    nmarkdown::StdioFileSystem files;
    nmarkdown::DocumentProbe probe;
    std::string error;
    const std::string path = std::string(NMARKDOWN_SOURCE_DIR) + "/samples/phase0.md";
    CHECK(files.probe(path.c_str(), probe, error));
    CHECK(error.empty());
    CHECK(probe.size > 100);
    CHECK(probe.bytes_sampled > 100);
    CHECK(probe.sample_hash != 2166136261U);

    std::vector<std::string> documents;
    const std::string samples = std::string(NMARKDOWN_SOURCE_DIR) + "/samples";
    bool truncated = true;
    CHECK(files.list_reader_documents(samples.c_str(), 32, documents, error,
                                      &truncated));
    CHECK(!truncated);
    CHECK(documents.size() >= 3);
    CHECK(std::find_if(documents.begin(), documents.end(),
                       [](const std::string& path) {
                           return path.size() >= 9 &&
                                  path.compare(path.size() - 9, 9, "phase0.md") == 0;
                       }) != documents.end());

    documents.clear();
    CHECK(files.list_reader_documents(samples.c_str(), 1, documents, error,
                                      &truncated));
    CHECK(documents.size() == 1);
    CHECK(truncated);

    std::vector<std::string> fonts;
    const std::string assets = std::string(NMARKDOWN_SOURCE_DIR) + "/assets/fonts";
    CHECK(files.list_font_files(assets.c_str(), 32, fonts, error, &truncated));
    CHECK(!truncated);
    CHECK(std::find_if(fonts.begin(), fonts.end(),
                       [](const std::string& path) {
                           constexpr std::string_view name =
                               "DejaVuSans.ttf";
                           return path.size() >= name.size() &&
                                  path.compare(path.size() - name.size(),
                                               name.size(), name) == 0;
                       }) != fonts.end());

    // The CJK distribution target uses the calculator's ordinary opaque
    // `.tns` wrapper.  The on-device font browser must discover that exact
    // double extension, not only desktop `.ttf` files.
    const std::string packaged_fonts =
        std::string(NMARKDOWN_BINARY_DIR) + "/fonts";
    fonts.clear();
    CHECK(files.list_font_files(packaged_fonts.c_str(), 1, fonts, error,
                                &truncated));
    CHECK(!truncated);
    constexpr std::string_view cjk_name =
        "SarasaFixedSC-Regular.ttf.tns";
    const auto cjk = std::find_if(
        fonts.begin(), fonts.end(), [cjk_name](const std::string& path) {
            return path.size() >= cjk_name.size() &&
                   path.compare(path.size() - cjk_name.size(),
                                cjk_name.size(), cjk_name) == 0;
        });
    CHECK(cjk != fonts.end());
    if (cjk != fonts.end()) {
        std::vector<std::uint8_t> cjk_bytes;
        CHECK(files.read_all(cjk->c_str(), 6U * 1024U * 1024U,
                             cjk_bytes, error));
        CHECK(cjk_bytes.size() == 6105504U);
        CHECK(files.read_all(cjk->c_str(), 12U * 1024U * 1024U,
                             cjk_bytes, error));
        CHECK(cjk_bytes.size() == 6105504U);
    }

    // Font discovery is synchronous on the calculator. Verify that a font in
    // the root is returned before a large unrelated subtree exhausts the scan
    // budget, and that the incomplete traversal is reported explicitly.
    namespace fs = std::filesystem;
    const fs::path scan_root =
        fs::path(NMARKDOWN_BINARY_DIR) / "font-picker-scan-budget";
    const fs::path noise_root = scan_root / "unrelated-documents";
    std::error_code filesystem_error;
    fs::remove_all(scan_root, filesystem_error);
    filesystem_error.clear();
    CHECK(fs::create_directories(noise_root, filesystem_error));
    const fs::path root_font = scan_root / "SarasaFixedSC-Regular.ttf.tns";
    FILE* root_font_file = std::fopen(root_font.string().c_str(), "wb");
    CHECK(root_font_file != nullptr);
    if (root_font_file != nullptr) std::fclose(root_font_file);
    for (int index = 0; index < 2200; ++index) {
        const fs::path noise = noise_root /
            ("document-" + std::to_string(index) + ".txt");
        FILE* file = std::fopen(noise.string().c_str(), "wb");
        CHECK(file != nullptr);
        if (file != nullptr) std::fclose(file);
    }
    fonts.clear();
    truncated = false;
    CHECK(files.list_font_files(scan_root.string().c_str(), 128, fonts,
                                error, &truncated));
    CHECK(truncated);
    CHECK(fonts.size() == 1);
    CHECK(!fonts.empty() && fonts.front() == root_font.string());
    fs::remove_all(scan_root, filesystem_error);

    // The unmodified upstream source is retained only for reproducibility. It
    // is deliberately rejected by both device guards; users receive the
    // calculator subset above.
    const std::string full_cjk = std::string(NMARKDOWN_SOURCE_DIR) +
        "/assets/fonts/SarasaFixedSC-Regular.ttf";
    std::vector<std::uint8_t> full_cjk_bytes;
    CHECK(!files.read_all(full_cjk.c_str(), 6U * 1024U * 1024U,
                          full_cjk_bytes, error));
    CHECK(error.find("size limit") != std::string::npos);
    CHECK(!files.read_all(full_cjk.c_str(), 12U * 1024U * 1024U,
                          full_cjk_bytes, error));
    CHECK(error.find("size limit") != std::string::npos);

    const std::string state_path =
        std::string(NMARKDOWN_BINARY_DIR) + "/stdio-atomic-state.bin";
    std::remove(state_path.c_str());
    std::remove((state_path + ".tmp").c_str());
    std::remove((state_path + ".bak").c_str());
    const std::vector<std::uint8_t> first{1, 2, 3};
    const std::vector<std::uint8_t> second{9, 8, 7, 6, 5};
    CHECK(files.write_atomic(state_path.c_str(), first.data(), first.size(), error));
    CHECK(files.write_atomic(state_path.c_str(), second.data(), second.size(), error));
    std::vector<std::uint8_t> stored;
    CHECK(files.read_all(state_path.c_str(), 1024, stored, error));
    CHECK(stored == second);
    std::remove(state_path.c_str());
}

void test_document_browser() {
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);

    nmarkdown::Viewer startup_viewer;
    startup_viewer.show_document_browser({});
    startup_viewer.render(surface);
    CHECK(startup_viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(startup_viewer.quit_requested());

    nmarkdown::Viewer startup_menu_viewer;
    startup_menu_viewer.show_document_browser({});
    CHECK(startup_menu_viewer.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    CHECK(startup_menu_viewer.quit_requested());

    nmarkdown::Viewer viewer;
    viewer.show_document_browser({"/documents/first.md.tns",
                                  "/documents/second.markdown.tns"});
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    std::string path;
    CHECK(viewer.take_document_open_request(path));
    CHECK(path == "/documents/second.markdown.tns");
    CHECK(!viewer.take_document_open_request(path));

    nmarkdown::DocumentProbe loaded_probe;
    loaded_probe.size = 128;
    viewer.set_document(loaded_probe);
    viewer.show_document_browser({});
    viewer.render(surface);
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(!viewer.quit_requested());
    viewer.render(surface);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    viewer.render(surface);

    viewer.show_document_browser({"/library/alpha/shared.md.tns",
                                  "/library/beta/shared.md.tns"});
    viewer.render(surface);  // Duplicate basenames include unique parent suffixes.
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));

    std::vector<std::string> capped;
    for (int index = 0; index < 256; ++index) {
        capped.push_back("/documents/item-" + std::to_string(index) + ".md.tns");
    }
    viewer.show_document_browser(capped, true);
    CHECK(viewer.document_browser_truncated());
    for (int index = 0; index < 60; ++index) {
        viewer.handle_event({nmarkdown::InputEventType::PageDown, 0});
    }
    viewer.render(surface);  // Includes the non-selectable cap warning row.
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.take_document_open_request(path));
    CHECK(path == capped.back());

    viewer.show_document_browser(capped, false);
    CHECK(!viewer.document_browser_truncated());
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
}

void test_empty_document_has_an_explicit_state() {
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, ""));
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    viewer.render(surface);
    const std::uint16_t paper = nmarkdown::rgb565(255, 255, 252);
    std::size_t visible_ink = 0;
    for (int y = 24; y < 110; ++y) {
        for (int x = 0; x < 320; ++x) {
            if (surface.pixel(x, y) != paper) ++visible_ink;
        }
    }
    CHECK(visible_ink > 20);
}

void test_startup_canvas_has_no_synthetic_document() {
    nmarkdown::Viewer viewer;
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    viewer.render(surface);
    const std::uint16_t paper = nmarkdown::rgb565(255, 255, 252);
    std::size_t unexpected_ink = 0;
    for (int y = 18; y < 238; ++y) {
        for (int x = 0; x < 320; ++x) {
            if (surface.pixel(x, y) != paper) ++unexpected_ink;
        }
    }
    CHECK(unexpected_ink == 0);
}

void test_loading_feedback_card_is_transient() {
    nmarkdown::Viewer viewer;
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    const std::uint16_t accent = nmarkdown::rgb565(61, 144, 214);

    viewer.render(surface);
    CHECK(surface.pixel(28, 82) != accent);
    viewer.show_loading_feedback("Opening document", "Parsing Markdown");
    CHECK(viewer.loading_feedback_visible());
    viewer.render(surface);
    CHECK(surface.pixel(28, 82) == accent);
    CHECK(surface.pixel(291, 157) == accent);
    CHECK(surface.pixel(50, 144) == accent);
    CHECK(surface.pixel(144, 144) != accent);

    viewer.show_loading_feedback("Opening document", "Parsing Markdown");
    viewer.render(surface);
    CHECK(surface.pixel(50, 144) != accent);
    CHECK(surface.pixel(144, 144) == accent);

    viewer.show_loading_feedback("Opening document", "Reading file", 50);
    viewer.render(surface);
    CHECK(surface.pixel(50, 144) == accent);
    CHECK(surface.pixel(150, 144) == accent);
    CHECK(surface.pixel(200, 144) != accent);

    viewer.clear_loading_feedback();
    CHECK(!viewer.loading_feedback_visible());
    viewer.render(surface);
    CHECK(surface.pixel(28, 82) != accent);
    CHECK(surface.pixel(291, 157) != accent);
}

void test_retained_base_frame_accelerates_modal_repaints() {
    std::string source = "# Retained frame\n\n";
    for (int line = 0; line < 40; ++line) {
        source += "A stable paragraph keeps the document frame non-empty.\n\n";
    }
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    viewer.render(surface);
    const std::vector<std::uint16_t> clean_frame = pixels;
    CHECK(viewer.retained_frame_fast_path_count() == 0);

    viewer.show_document_browser({"/documents/one.md.tns",
                                  "/documents/two.md.tns"});
    viewer.render(surface);
    CHECK(viewer.retained_frame_fast_path_count() == 1);
    CHECK(pixels != clean_frame);

    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    viewer.render(surface);
    CHECK(viewer.retained_frame_fast_path_count() == 2);

    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    viewer.render(surface);
    CHECK(viewer.retained_frame_fast_path_count() == 3);
    CHECK(pixels == clean_frame);

    // A real document movement invalidates the snapshot. Page/scroll rendering
    // remains on its existing path and is never replaced by stale pixels.
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    viewer.render(surface);
    CHECK(viewer.retained_frame_fast_path_count() == 3);
    CHECK(pixels != clean_frame);
}

void test_jump_and_font_refresh_paint_the_first_frame() {
    std::string source = "# Start\n\n";
    const std::string long_paragraph =
        "A deliberately long paragraph has enough repeated words to replace "
        "the virtual layout estimate with several measured lines on the narrow "
        "calculator display. It makes anchor movement during lazy layout large "
        "enough to expose stale first-frame paint lists.\n\n";
    for (int index = 0; index < 48; ++index) source += long_paragraph;
    source += "## Target refresh heading\n\nDestination body text.\n\n";
    for (int index = 0; index < 12; ++index) source += long_paragraph;

    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    CHECK(viewer.navigate_to_anchor("target-refresh-heading"));
    CHECK(viewer.scroll_y() > 0);

    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    const std::uint16_t accent = nmarkdown::rgb565(61, 144, 214);
    const auto body_accent_pixels = [&]() {
        std::size_t count = 0;
        for (int y = 18; y < 238; ++y) {
            for (int x = 0; x < 320; ++x) {
                if (surface.pixel(x, y) == accent) ++count;
            }
        }
        return count;
    };

    // The destination heading must be present in the first frame after a
    // semantic jump; requiring another scroll event is the white-frame bug.
    viewer.render(surface);
    CHECK(body_accent_pixels() > 8);

    nmarkdown::FontPack pack;
    std::string error;
    CHECK(pack.load_from_memory(nmarkdown::kCoreFontPack,
                                nmarkdown::kCoreFontPackSize, error));
    const nmarkdown::FontPackFace* face = pack.face(0);
    CHECK(face != nullptr);
    if (face == nullptr) return;
    std::vector<std::uint8_t> body_font(face->font_data,
                                        face->font_data + face->font_size);
    CHECK(viewer.set_font(nmarkdown::FontRole::BodySans,
                          std::move(body_font), "Refresh body", error));
    std::fill(pixels.begin(), pixels.end(), 0);
    viewer.render(surface);
    CHECK(body_accent_pixels() > 8);
}

void test_font_pack_menu() {
    nmarkdown::Viewer viewer;
    std::string error;
    CHECK(viewer.text_ready());
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    for (int row = 0; row < 12; ++row) {
        CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    }
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.take_font_menu_request());
    CHECK(!viewer.take_font_menu_request());

    const std::string font_path = "/documents/fonts/screen-cjk.ttf.tns";
    nmarkdown::FontFaceCatalogEntry screen_font;
    screen_font.family = "Screen CJK";
    screen_font.path = font_path;
    screen_font.has_cjk = true;
    viewer.show_font_manager({screen_font}, {});
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    for (int row = 0; row < 3; ++row) {
        CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    }
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    std::array<std::string, nmarkdown::kExternalFontRoleCount> selected;
    CHECK(viewer.take_font_assignments(selected));
    CHECK(selected[3] == font_path);
    CHECK(!viewer.take_font_assignments(selected));

    std::vector<nmarkdown::FontFaceCatalogEntry> fonts;
    for (int index = 0; index < 8; ++index) {
        nmarkdown::FontFaceCatalogEntry font;
        font.family = "Family " + std::to_string(index);
        font.path = "/documents/fonts/family-" +
                    std::to_string(index) + ".ttf.tns";
        font.has_latin = true;
        fonts.push_back(std::move(font));
    }
    viewer.show_font_manager(fonts, {}, true);
    CHECK(viewer.font_browser_truncated());
    for (int count = 0; count < 8; ++count) {
        viewer.handle_event({nmarkdown::InputEventType::PageDown, 0});
    }
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineUp, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineUp, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.take_font_assignments(selected));
    CHECK(selected[0] == fonts.back().path);

    viewer.show_font_manager(fonts, {}, false);
    CHECK(!viewer.font_browser_truncated());
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));

    nmarkdown::FontPack core;
    CHECK(core.load_from_memory(nmarkdown::kCoreFontPack,
                                nmarkdown::kCoreFontPackSize, error));
    const nmarkdown::FontPackFace* core_face = core.face(0);
    CHECK(core_face != nullptr);
    std::vector<std::uint8_t> raw_font;
    if (core_face != nullptr) {
        raw_font.assign(core_face->font_data,
                        core_face->font_data + core_face->font_size);
    }
    CHECK(viewer.set_font(nmarkdown::FontRole::Monospace,
                          raw_font, "DejaVu Sans", error));
    CHECK(error.empty());
    CHECK(viewer.text_ready());

    const std::vector<std::uint8_t> invalid_font{0, 1, 2, 3};
    CHECK(!viewer.set_font(nmarkdown::FontRole::Cjk,
                           invalid_font, "broken", error));
    CHECK(!error.empty());
    CHECK(viewer.text_ready());

    error.clear();
    CHECK(viewer.set_font(nmarkdown::FontRole::Monospace, {}, {}, error));
    CHECK(error.empty());

    nmarkdown::Viewer missing_cjk;
    CHECK(load_markdown(missing_cjk, u8"中文排版测试。\n"));
    // A CJK document with no selected fallback opens one explanatory prompt.
    CHECK(missing_cjk.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(!missing_cjk.quit_requested());
    missing_cjk.handle_event({nmarkdown::InputEventType::Back, 0});
    CHECK(missing_cjk.quit_requested());

}

void test_loaded_sarasa_cjk_picker_remains_responsive() {
    const std::string cjk_path = std::string(NMARKDOWN_BINARY_DIR) +
        "/fonts/SarasaFixedSC-Regular.ttf.tns";
    nmarkdown::StdioFileSystem files;
    nmarkdown::DocumentProbe probe;
    std::shared_ptr<nmarkdown::RandomAccessData> source;
    std::string error;
    CHECK(files.probe(cjk_path.c_str(), probe, error));
    CHECK(files.open_random_access(cjk_path.c_str(), source, error));
    CHECK(source != nullptr);
    CHECK(probe.size == 6105504U);
    if (source == nullptr) return;

    const auto registry_for_cjk = [&]() {
        nmarkdown::FontRegistryState registry;
        registry.fonts.push_back(
            {9300, nullptr, source, probe.sample_hash});
        registry.roles[static_cast<std::size_t>(
            nmarkdown::external_font_role_index(
                nmarkdown::FontRole::Cjk))] = 9300;
        return registry;
    };
    std::array<std::string, nmarkdown::kExternalFontRoleCount> labels{};
    labels[3] = "Sarasa Fixed SC Regular";

    nmarkdown::Viewer viewer;
    CHECK(viewer.set_font_registry(registry_for_cjk(), labels, error));
    CHECK(error.empty());
    CHECK(viewer.external_font_bytes(nmarkdown::FontRole::Cjk) == 6105504U);

    // A streamed role is present even though it intentionally has no
    // whole-file byte vector. CJK documents must not show the missing-font
    // prompt in this representation.
    nmarkdown::Viewer streamed_document;
    CHECK(streamed_document.set_font_registry(registry_for_cjk(), labels,
                                              error));
    CHECK(load_markdown(streamed_document, u8"中文排版测试。\n"));
    streamed_document.handle_event({nmarkdown::InputEventType::Back, 0});
    CHECK(streamed_document.quit_requested());

    std::vector<nmarkdown::FontFaceCatalogEntry> fonts;
    nmarkdown::FontFaceCatalogEntry sarasa;
    sarasa.family = "Sarasa Fixed SC";
    sarasa.path = cjk_path;
    sarasa.has_cjk = true;
    sarasa.fixed_pitch = true;
    fonts.push_back(std::move(sarasa));
    for (int index = 1; index < 128; ++index) {
        nmarkdown::FontFaceCatalogEntry font;
        font.family = "Family " + std::to_string(index);
        font.path = "/documents/family-" +
                    std::to_string(index) + "/shared.ttf.tns";
        font.has_cjk = true;
        fonts.push_back(std::move(font));
    }
    std::array<std::string, nmarkdown::kExternalFontRoleCount> active_paths{};
    active_paths[3] = cjk_path;
    viewer.show_font_manager(fonts, active_paths, true);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));

    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    viewer.render(surface);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    viewer.render(surface);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(!viewer.quit_requested());
}

void test_viewer_scroll_and_render() {
    nmarkdown::Viewer viewer;
    nmarkdown::DocumentProbe probe;
    probe.size = 9000;
    probe.sample_hash = 0x12345678U;
    viewer.set_document(probe);
    CHECK(viewer.document_loaded());
    CHECK(viewer.text_ready());
    CHECK(viewer.total_pages() > 1);
    CHECK(viewer.current_page() == 1);

    CHECK(viewer.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    CHECK(viewer.scroll_y() > 0);
    CHECK(viewer.current_page() > 1);
    for (int count = 0; count < 200; ++count) {
        viewer.handle_event({nmarkdown::InputEventType::PageDown, 0});
    }
    CHECK(viewer.scroll_y() == viewer.max_scroll_y());
    CHECK(viewer.current_page() == viewer.total_pages());
    viewer.handle_event({nmarkdown::InputEventType::PageUp, 0});
    CHECK(viewer.scroll_y() < viewer.max_scroll_y());
    CHECK(viewer.current_page() < viewer.total_pages());

    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    viewer.render(surface);
    const nmarkdown::GlyphCacheStats first_render = viewer.glyph_cache_stats();
    CHECK(first_render.misses > 0);
    viewer.render(surface);
    const nmarkdown::GlyphCacheStats second_render = viewer.glyph_cache_stats();
    CHECK(second_render.hits > first_render.hits);
    CHECK(second_render.misses == first_render.misses);
    CHECK(surface.pixel(0, 0) == nmarkdown::rgb565(59, 171, 111));
    CHECK(surface.pixel(0, 1) == nmarkdown::rgb565(59, 171, 111));
    CHECK(surface.pixel(319, 0) == nmarkdown::rgb565(191, 197, 207));
    CHECK(surface.pixel(0, 2) == nmarkdown::rgb565(25, 45, 74));
    CHECK(surface.pixel(0, 17) == nmarkdown::rgb565(25, 45, 74));
    const std::uint16_t paper = nmarkdown::rgb565(255, 255, 252);
    CHECK(surface.pixel(0, 239) == paper);
    CHECK(surface.pixel(0, 20) == paper);
    CHECK(surface.pixel(319, 20) == paper);
    for (int y = 238; y < 240; ++y) {
        for (int x = 0; x < 320; ++x) {
            CHECK(surface.pixel(x, y) == paper);
        }
    }

    const bool initial_theme = viewer.dark_theme();
    const int activate_scroll = viewer.scroll_y();
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.dark_theme() == initial_theme);
    CHECK(viewer.scroll_y() == activate_scroll);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.dark_theme());
    const int previous_size = viewer.body_pixel_size();
    CHECK(viewer.handle_event({nmarkdown::InputEventType::IncreaseFont, 0}));
    CHECK(viewer.body_pixel_size() == previous_size + 1);
    nmarkdown::ReaderPerformanceMetrics metrics;
    metrics.document_load_parse_ms = 7;
    metrics.first_visible_render_ms = 11;
    metrics.last_visible_render_ms = 4;
    metrics.peak_visible_render_ms = 12;
    metrics.last_present_ms = 2;
    metrics.peak_present_ms = 3;
    viewer.set_performance_metrics(metrics);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenDiagnostics, 0}));
    viewer.render(surface);
    CHECK(surface.pixel(14, 221) == nmarkdown::rgb565(76, 166, 231));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(!viewer.quit_requested());
}

void test_progress_bar_tracks_continuous_position_in_both_modes() {
    constexpr int kScreenWidth = 320;
    const std::uint16_t progress = nmarkdown::rgb565(59, 171, 111);
    const std::uint16_t track = nmarkdown::rgb565(191, 197, 207);
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    nmarkdown::DocumentProbe probe;
    probe.size = 9000;

    nmarkdown::Viewer scroll;
    scroll.set_document(probe);
    scroll.render(surface);
    CHECK(scroll.scroll_y() == 0);
    CHECK(leading_color_width(surface, progress) == 0);
    CHECK(surface.pixel(0, 0) == track);
    CHECK(surface.pixel(319, 1) == track);

    CHECK(scroll.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    scroll.render(surface);
    const int expected_scroll_width = static_cast<int>(
        static_cast<std::int64_t>(kScreenWidth) * scroll.scroll_y() /
        scroll.max_scroll_y());
    CHECK(expected_scroll_width > 0);
    CHECK(expected_scroll_width < kScreenWidth);
    CHECK(leading_color_width(surface, progress) == expected_scroll_width);
    CHECK(surface.pixel(expected_scroll_width, 0) == track);

    // Both touchpad modes traverse the same reflowed document. Switching axes
    // at a nonzero position therefore keeps the same continuous progress.
    nmarkdown::Viewer switching;
    switching.set_document(probe);
    CHECK(switching.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    switching.render(surface);
    CHECK(switching.max_scroll_y() > 0);
    if (switching.max_scroll_y() > 0) {
        CHECK(leading_color_width(surface, progress) ==
              static_cast<int>(static_cast<std::int64_t>(kScreenWidth) *
                               switching.scroll_y() /
                               switching.max_scroll_y()));
    }
    const int switching_width = leading_color_width(surface, progress);
    switching.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);
    switching.render(surface);
    CHECK(leading_color_width(surface, progress) == switching_width);
    switching.set_reading_mode(nmarkdown::ReadingMode::VerticalScroll);
    switching.render(surface);
    CHECK(switching.max_scroll_y() > 0);
    if (switching.max_scroll_y() > 0) {
        CHECK(leading_color_width(surface, progress) ==
              static_cast<int>(static_cast<std::int64_t>(kScreenWidth) *
                               switching.scroll_y() /
                               switching.max_scroll_y()));
    }

    for (int count = 0; count < 200; ++count) {
        scroll.handle_event({nmarkdown::InputEventType::PageDown, 0});
    }
    CHECK(scroll.scroll_y() == scroll.max_scroll_y());
    scroll.render(surface);
    CHECK(leading_color_width(surface, progress) == kScreenWidth);
    CHECK(surface.pixel(319, 1) == progress);

    nmarkdown::Viewer horizontal;
    horizontal.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);
    horizontal.set_document(probe);
    horizontal.render(surface);
    CHECK(leading_color_width(surface, progress) == 0);
    CHECK(surface.pixel(0, 0) == track);

    CHECK(horizontal.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    horizontal.render(surface);
    const int expected_horizontal_width = static_cast<int>(
        static_cast<std::int64_t>(kScreenWidth) * horizontal.scroll_y() /
        horizontal.max_scroll_y());
    CHECK(expected_horizontal_width > 0);
    CHECK(expected_horizontal_width < kScreenWidth);
    CHECK(leading_color_width(surface, progress) == expected_horizontal_width);
    CHECK(surface.pixel(expected_horizontal_width, 1) == track);

    for (int count = 0; count < 200; ++count) {
        horizontal.handle_event({nmarkdown::InputEventType::PageDown, 0});
    }
    CHECK(horizontal.scroll_y() == horizontal.max_scroll_y());
    horizontal.render(surface);
    CHECK(leading_color_width(surface, progress) == kScreenWidth);
    CHECK(surface.pixel(319, 0) == progress);

    // A true one-viewport document exercises the zero-denominator rule in both
    // reading modes: with nowhere to move, it remains at the 0% start state.
    nmarkdown::Viewer single_page;
    CHECK(load_markdown(single_page, "One short line.\n"));
    single_page.render(surface);
    CHECK(single_page.max_scroll_y() == 0);
    CHECK(leading_color_width(surface, progress) == 0);
    single_page.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);
    single_page.render(surface);
    CHECK(single_page.total_pages() == 1);
    CHECK(leading_color_width(surface, progress) == 0);

    // A load failure is an alert status rather than reading progress, so it
    // deliberately retains the full-width red strip.
    nmarkdown::Viewer error;
    const std::uint16_t failure = nmarkdown::rgb565(213, 76, 84);
    error.set_document_error("broken");
    error.render(surface);
    CHECK(leading_color_width(surface, failure) == kScreenWidth);
    CHECK(surface.pixel(319, 1) == failure);
    const std::vector<std::uint16_t> scroll_error_frame = pixels;
    error.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);
    error.render(surface);
    CHECK(pixels == scroll_error_frame);  // No meaningless page label on errors.
}

void test_settings_theme_repaint_stays_responsive() {
    std::string source =
        "# Theme transition\n\n"
        "Body text with *italics*, `code`, and $x^2 + y^2 = z^2$.\n\n";
    for (int index = 0; index < 24; ++index) {
        source += "A wrapping paragraph keeps the document scrollable while "
                  "the settings overlay changes between light and dark.\n\n";
    }

    std::unique_ptr<nmarkdown::MarkdownDocument> document(
        new nmarkdown::MarkdownDocument());
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        *document, error));
    nmarkdown::DocumentProbe probe;
    probe.size = source.size();
    nmarkdown::Viewer viewer;
    CHECK(viewer.set_markdown_document(std::move(document), probe, error));
    // A net downward step followed by one step up leaves the reader scrolled
    // with the auto-hiding title bar revealed for the header assertions.
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineUp, 0}));
    const int initial_scroll = viewer.scroll_y();
    CHECK(initial_scroll > 0);

    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    viewer.render(surface);
    viewer.clear_dirty();
    CHECK(surface.pixel(0, 2) == nmarkdown::rgb565(25, 45, 74));
    CHECK(surface.pixel(0, 20) == nmarkdown::blend565(
              nmarkdown::rgb565(255, 255, 252), nmarkdown::rgb565(0, 0, 0), 64));
    const nmarkdown::GlyphCacheStats light_glyphs = viewer.glyph_cache_stats();
    const nmarkdown::FormulaCacheStats light_formulas = viewer.formula_cache_stats();
    const int initial_max_scroll = viewer.max_scroll_y();
    CHECK(light_glyphs.entries > 0);
    CHECK(light_formulas.entries > 0);
    const std::vector<std::uint16_t> light_settings_frame = pixels;

    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.dark_theme());
    CHECK(viewer.dirty());
    CHECK(viewer.scroll_y() == initial_scroll);
    CHECK(viewer.max_scroll_y() == initial_max_scroll);
    viewer.render(surface);
    const nmarkdown::GlyphCacheStats dark_glyphs = viewer.glyph_cache_stats();
    const nmarkdown::FormulaCacheStats dark_formulas = viewer.formula_cache_stats();
    CHECK(dark_glyphs.hits > light_glyphs.hits);
    CHECK(dark_glyphs.entries >= light_glyphs.entries);
    CHECK(dark_formulas.entries == light_formulas.entries);
    // While Settings remains open, only its opaque panel is repainted. The
    // header and covered document stay byte-for-byte unchanged.
    CHECK(surface.pixel(0, 2) == light_settings_frame[2 * 320]);
    CHECK(surface.pixel(0, 20) == light_settings_frame[20 * 320]);
    CHECK(surface.pixel(22, 44) == nmarkdown::rgb565(76, 166, 231));

    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    viewer.render(surface);
    CHECK(surface.pixel(0, 2) == nmarkdown::rgb565(12, 27, 48));
    CHECK(surface.pixel(0, 20) == nmarkdown::rgb565(29, 35, 45));
    // Net downward movement with a final upward step keeps the auto-hiding
    // title bar revealed for the header assertions below.
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineUp, 0}));
    const int scrolled_after_toggle = viewer.scroll_y();
    CHECK(scrolled_after_toggle > initial_scroll);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    viewer.render(surface);
    const std::vector<std::uint16_t> dark_settings_frame = pixels;
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(!viewer.dark_theme());
    CHECK(viewer.scroll_y() == scrolled_after_toggle);
    viewer.render(surface);
    CHECK(surface.pixel(0, 2) == dark_settings_frame[2 * 320]);
    CHECK(surface.pixel(0, 20) == dark_settings_frame[20 * 320]);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    viewer.render(surface);
    CHECK(surface.pixel(0, 2) == nmarkdown::rgb565(25, 45, 74));
    CHECK(surface.pixel(0, 20) == nmarkdown::rgb565(255, 255, 252));
}

void test_layout_setting_reflow_waits_for_settings_close() {
    std::string source = "# Deferred reflow\n\n";
    for (int index = 0; index < 48; ++index) {
        source += "Several words wrap across the calculator display so a one "
                  "pixel font increase changes the measured document height.\n\n";
    }
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    viewer.render(surface);
    const int initial_max_scroll = viewer.max_scroll_y();

    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    viewer.render(surface);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    viewer.render(surface);
    const std::vector<std::uint16_t> font_row_frame = pixels;

    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.body_pixel_size() == 16);
    viewer.render(surface);
    CHECK(viewer.max_scroll_y() == initial_max_scroll);
    CHECK(surface.pixel(0, 2) == font_row_frame[2 * 320]);
    CHECK(surface.pixel(0, 120) == font_row_frame[120 * 320]);

    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    viewer.render(surface);
    CHECK(viewer.max_scroll_y() > initial_max_scroll);
}

void test_decorated_blocks_use_exact_symmetric_outer_margins() {
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer,
                        "```text\n"
                        "symmetric margin probe\n"
                        "```\n"));

    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    for (int row = 0; row < 3; ++row) {
        CHECK(viewer.handle_event(
            {nmarkdown::InputEventType::ScrollLineDown, 0}));
    }
    for (int pixel = 5; pixel < 10; ++pixel) {
        CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    }
    CHECK(viewer.side_margin_px() == 10);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));

    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    viewer.render(surface);
    const std::uint16_t paper = nmarkdown::rgb565(255, 255, 252);
    const std::uint16_t border = nmarkdown::rgb565(191, 197, 207);
    bool found_symmetric_border = false;
    for (int y = 18; y < 238; ++y) {
        if (surface.pixel(10, y) == border &&
            surface.pixel(309, y) == border &&
            surface.pixel(9, y) == paper &&
            surface.pixel(310, y) == paper) {
            found_symmetric_border = true;
            break;
        }
    }
    CHECK(found_symmetric_border);

    nmarkdown::Viewer rule_viewer;
    CHECK(load_markdown(rule_viewer, "---\n"));
    std::fill(pixels.begin(), pixels.end(), 0);
    rule_viewer.render(surface);
    bool found_full_width_rule = false;
    for (int y = 18; y < 238; ++y) {
        if (surface.pixel(5, y) == border &&
            surface.pixel(314, y) == border &&
            surface.pixel(160, y) == border &&
            surface.pixel(4, y) == paper &&
            surface.pixel(315, y) == paper) {
            found_full_width_rule = true;
            break;
        }
    }
    CHECK(found_full_width_rule);
}

void test_modal_input_isolation_and_escape_order() {
    std::string source;
    for (int index = 0; index < 40; ++index) {
        source += "A long reader paragraph keeps the document scrollable for modal tests.\n\n";
    }
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    const int initial_scroll = viewer.scroll_y();
    const int initial_size = viewer.body_pixel_size();
    const bool initial_theme = viewer.dark_theme();

    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::PointerScroll, 80}));
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::IncreaseFont, 0}));
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::ToggleBookmark, 0}));
    CHECK(viewer.scroll_y() == initial_scroll);
    CHECK(viewer.body_pixel_size() == initial_size);
    CHECK(viewer.reader_state(9).bookmarks.empty());
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    CHECK(viewer.scroll_y() > initial_scroll);

    const int before_help = viewer.scroll_y();
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.scroll_y() == before_help);
    CHECK(viewer.dark_theme() == initial_theme);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    CHECK(viewer.scroll_y() > before_help);

    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenDocument, 0}));
    CHECK(viewer.take_document_browser_request());
    CHECK(!viewer.take_document_browser_request());
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));

    viewer.show_font_manager({}, {});
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::OpenDocument, 0}));
    CHECK(!viewer.take_document_browser_request());
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));

    const std::string code =
        "```cpp\n"
        "const char* wide = \"a line far wider than the calculator viewport so it can pan\";\n"
        "```\n";
    nmarkdown::Viewer focused;
    CHECK(load_markdown(focused, code));
    CHECK(select_code_block_pan_mode(focused));
    CHECK(focused.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(focused.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    CHECK(focused.handle_event({nmarkdown::InputEventType::Back, 0}));
    // Esc closed the visible Settings layer; the underlying wide focus remains.
    CHECK(focused.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(focused.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(!focused.quit_requested());
}

void test_toc_bookmark_empty_state_is_safe() {
    std::string source = "# Alpha\n\n";
    for (int index = 0; index < 20; ++index) source += "Alpha filler.\n\n";
    source += "## Beta\n\nBeta destination.\n";
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));

    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::PointerScroll, 90}));
    CHECK(viewer.scroll_y() == 0);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));

    CHECK(viewer.handle_event({nmarkdown::InputEventType::ToggleBookmark, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ToggleBookmark, 0}));
    CHECK(viewer.reader_state(7).bookmarks.empty());

    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    const int before_activate = viewer.scroll_y();
    const bool theme = viewer.dark_theme();
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.scroll_y() == before_activate);
    CHECK(viewer.dark_theme() == theme);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
}

void test_vertical_and_horizontal_touchpad_modes() {
    nmarkdown::DocumentProbe probe;
    probe.size = 9000;
    probe.sample_hash = 0x89ABCDEFU;
    nmarkdown::Viewer viewer;
    viewer.set_document(probe);
    CHECK(viewer.reading_mode() == nmarkdown::ReadingMode::VerticalScroll);

    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    viewer.render(surface);
    const std::uint16_t header = nmarkdown::rgb565(25, 45, 74);
    std::size_t scroll_right_header_ink = 0;
    for (int y = 2; y < 18; ++y) {
        for (int x = 270; x < 316; ++x) {
            if (surface.pixel(x, y) != header) ++scroll_right_header_ink;
        }
    }
    CHECK(scroll_right_header_ink == 0);  // Neither mode shows a page number.

    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    for (int row = 0; row < 8; ++row) {
        CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    }
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.reading_mode() == nmarkdown::ReadingMode::HorizontalScroll);

    viewer.render(surface);
    std::size_t horizontal_right_header_ink = 0;
    for (int y = 2; y < 18; ++y) {
        for (int x = 270; x < 316; ++x) {
            if (surface.pixel(x, y) != header) ++horizontal_right_header_ink;
        }
    }
    CHECK(horizontal_right_header_ink == 0);

    CHECK(viewer.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    const int keyed_step = viewer.scroll_y();
    // This synthetic probe has no line geometry to align against, so its
    // fallback screen step is exactly the 220 px document viewport.
    CHECK(keyed_step == 238);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PageUp, 0}));
    CHECK(viewer.scroll_y() == 0);

    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.scroll_y() == 18);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineUp, 0}));
    CHECK(viewer.scroll_y() == 0);

    CHECK(!viewer.handle_event({nmarkdown::InputEventType::PanLeft, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    const int horizontal_key_step = viewer.scroll_y();
    CHECK(horizontal_key_step > 0);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanLeft, 0}));
    CHECK(viewer.scroll_y() == 0);

    CHECK(!viewer.handle_event({nmarkdown::InputEventType::PointerScroll, -1}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PointerPan, -18}));
    CHECK(viewer.scroll_y() == 18);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PointerPan, 18}));
    CHECK(viewer.scroll_y() == 0);

    CHECK(viewer.handle_event({nmarkdown::InputEventType::SwipeDown, 0}));
    const int vertical_swipe_step = viewer.scroll_y();
    CHECK(vertical_swipe_step > 0);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::SwipeUp, 0}));
    CHECK(viewer.scroll_y() == 0);

    for (int count = 0; count < 100; ++count) {
        viewer.handle_event({nmarkdown::InputEventType::PageDown, 0});
    }
    CHECK(viewer.scroll_y() == viewer.max_scroll_y());
    const nmarkdown::ReaderState state = viewer.reader_state(17);
    CHECK(state.reading_mode == nmarkdown::ReadingMode::HorizontalScroll);
}

void test_horizontal_scroll_restore_has_visible_content() {
    std::string source = "# Restored page\n\n";
    for (int index = 0; index < 55; ++index) {
        source += "Visible paragraph " + std::to_string(index) +
                  " keeps the final restored page populated.\n\n";
    }
    const std::uint64_t identity = nmarkdown::document_identity(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size());
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);

    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    viewer.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);
    for (int index = 0; index < 80; ++index) {
        viewer.handle_event({nmarkdown::InputEventType::PageDown, 0});
        viewer.render(surface);
    }
    CHECK(viewer.current_page() == viewer.total_pages());
    const nmarkdown::ReaderState state = viewer.reader_state(identity);

    nmarkdown::Viewer restored;
    CHECK(load_markdown(restored, source));
    CHECK(restored.apply_reader_state(state, identity));
    restored.render(surface);
    CHECK(restored.current_page() == restored.total_pages());
    CHECK(leading_color_width(surface, nmarkdown::rgb565(59, 171, 111)) ==
          surface.width());
    const std::uint16_t paper = nmarkdown::rgb565(255, 255, 252);
    std::size_t visible_ink = 0;
    for (int y = 18; y < 238; ++y) {
        for (int x = 0; x < 320; ++x) {
            if (surface.pixel(x, y) != paper) ++visible_ink;
        }
    }
    CHECK(visible_ink > 20);
}

void test_horizontal_scroll_lazy_growth_reaches_document_end() {
    const std::string source =
        "# Horizontal Scroll verification\n\n"
        "This screen demonstrates context-preserving navigation on the calculator. "
        "The header keeps the filename unobstructed across its available width.\n\n"
        "This second paragraph fills the first screen with enough text to make the "
        "next screen step a real layout boundary rather than an empty frame.\n\n"
        "## Next screen boundary\n\n"
        "Tab or an upward swipe advances while retaining visual context. The "
        "next frame starts at a complete line instead of clipping a heading.\n\n"
        "The progress strip remains two pixels high and follows the continuous "
        "reading position without calculating or displaying page counts.\n\n"
        "## Final section\n\n"
        "Additional content verifies that lazy layout growth still reaches the "
        "exact end of the document.\n";
    nmarkdown::Viewer viewer;
    viewer.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);
    CHECK(load_markdown(viewer, source));
    std::vector<std::uint16_t> pixels(320 * 240, 0);
    nmarkdown::Surface565 surface(pixels.data(), 320, 240, 320);
    const std::uint16_t progress = nmarkdown::rgb565(59, 171, 111);
    const std::uint16_t track = nmarkdown::rgb565(191, 197, 207);
    viewer.render(surface);
    CHECK(leading_color_width(surface, progress) == 0);
    CHECK(surface.pixel(0, 0) == track);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PageDown, 0}));
    viewer.render(surface);
    const int expected_progress_width = static_cast<int>(
        static_cast<std::int64_t>(surface.width()) * viewer.scroll_y() /
        viewer.max_scroll_y());
    CHECK(expected_progress_width > 0);
    CHECK(leading_color_width(surface, progress) == expected_progress_width);
    if (expected_progress_width < surface.width()) {
        CHECK(surface.pixel(expected_progress_width, 1) == track);
    }
    int page_guard = 0;
    while (viewer.scroll_y() < viewer.max_scroll_y() && page_guard < 20) {
        const int before_scroll = viewer.scroll_y();
        CHECK(viewer.handle_event({nmarkdown::InputEventType::PageDown, 0}));
        CHECK(viewer.scroll_y() > before_scroll);
        viewer.render(surface);
        ++page_guard;
    }
    CHECK(page_guard < 20);
    CHECK(viewer.scroll_y() == viewer.max_scroll_y());
    CHECK(leading_color_width(surface, progress) == surface.width());
}

void test_markdown_toc_reflow_and_state() {
    std::string source = "# Start\n\n";
    for (int index = 0; index < 40; ++index) {
        source += "A paragraph before the second heading with wrapping text.\n\n";
    }
    source += "## Target\n\nThe destination paragraph.\n\n";
    for (int index = 0; index < 20; ++index) {
        source += "A paragraph before the final heading with wrapping text.\n\n";
    }
    source += "## Final\n\nThe second destination paragraph.\n";
    auto parse = [&source]() {
        std::unique_ptr<nmarkdown::MarkdownDocument> document(
            new nmarkdown::MarkdownDocument());
        std::string error;
        CHECK(nmarkdown::parse_markdown(
            reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
            *document, error));
        return document;
    };

    nmarkdown::DocumentProbe probe;
    probe.size = source.size();
    nmarkdown::Viewer viewer;
    std::string error;
    CHECK(viewer.set_markdown_document(parse(), probe, error));
    CHECK(viewer.has_markdown_document());
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    std::vector<std::uint16_t> toc_pixels(320 * 240, 0);
    nmarkdown::Surface565 toc_surface(toc_pixels.data(), 320, 240, 320);
    viewer.render(toc_surface);  // Opening and painting the TOC must stay responsive.
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.scroll_y() == 0);  // The arrow selected a TOC row.
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.scroll_y() > 0);
    const int first_toc_jump = viewer.scroll_y();
    viewer.render(toc_surface);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    viewer.render(toc_surface);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.scroll_y() > first_toc_jump);
    viewer.render(toc_surface);  // A second distinct TOC jump must not hang.
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSearch, 0}));
    for (char character : std::string("target")) {
        CHECK(viewer.handle_event(
            {nmarkdown::InputEventType::TextInput, character}));
    }
    CHECK(viewer.search_query() == "target");
    CHECK(viewer.search_result_count() == 1);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.scroll_y() > 0);
    CHECK(viewer.has_active_search_match());
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ToggleBookmark, 0}));
    for (int index = 0; index < 100; ++index) {
        viewer.handle_event({nmarkdown::InputEventType::PageUp, 0});
    }
    CHECK(viewer.scroll_y() == 0);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenMenu, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.scroll_y() > 0);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.dark_theme());
    CHECK(viewer.handle_event({nmarkdown::InputEventType::IncreaseFont, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));

    const std::uint64_t identity = nmarkdown::document_identity(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size());
    const nmarkdown::ReaderState state = viewer.reader_state(identity);
    CHECK(state.position.source_offset > 0);
    CHECK(state.bookmarks.size() == 1);
    CHECK(state.dark_theme);
    CHECK(state.font_size == 16);
    CHECK(state.line_gap == 0);
    CHECK(state.side_margin == 6);
    CHECK(state.table_mode == 1);
    CHECK(!state.code_wrap);  // The settings exercise selected Pan from Wrap.
    CHECK(state.high_contrast);
    CHECK(!state.natural_swiping);
    CHECK(state.natural_scrolling);

    nmarkdown::Viewer restored;
    CHECK(restored.set_markdown_document(parse(), probe, error));
    CHECK(!restored.apply_reader_state(state, identity + 1));
    CHECK(restored.apply_reader_state(state, identity));
    CHECK(restored.dark_theme());
    CHECK(restored.body_pixel_size() == 16);
    CHECK(restored.line_gap_px() == 0);
    CHECK(restored.side_margin_px() == 6);
    CHECK(restored.scroll_y() > 0);
    CHECK(restored.reader_state(identity).table_mode == 1);
    CHECK(!restored.reader_state(identity).code_wrap);
    CHECK(restored.high_contrast());
    CHECK(!restored.natural_swiping());
    CHECK(restored.natural_scrolling());
    restored.render(toc_surface);
    CHECK(restored.max_scroll_y() > 0);
    if (restored.max_scroll_y() > 0) {
        const int expected_restored_progress = static_cast<int>(
            static_cast<std::int64_t>(toc_surface.width()) *
            restored.scroll_y() / restored.max_scroll_y());
        const std::uint16_t dark_high_contrast_progress =
            nmarkdown::rgb565(80, 255, 120);
        CHECK(leading_color_width(toc_surface, dark_high_contrast_progress) ==
              expected_restored_progress);
    }
}

void test_wide_block_focus() {
    const std::string source =
        "```cpp\n"
        "const char* value = \"a very long source-code line that needs horizontal panning\";\n"
        "```\n";
    std::unique_ptr<nmarkdown::MarkdownDocument> document(
        new nmarkdown::MarkdownDocument());
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        *document, error));
    nmarkdown::Viewer viewer;
    nmarkdown::DocumentProbe probe;
    probe.size = source.size();
    CHECK(viewer.set_markdown_document(std::move(document), probe, error));
    CHECK(select_code_block_pan_mode(viewer));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.pan_x() == 12);
    for (int index = 0; index < 500; ++index) {
        viewer.handle_event({nmarkdown::InputEventType::PanRight, 0});
    }
    const int final_pan = viewer.pan_x();
    CHECK(final_pan > 12);
    CHECK(final_pan < 2048);
    CHECK(!viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.pan_x() == final_pan);
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(viewer.pan_x() == 0);
    // Horizontal input stays reserved for panning while the wide block is in
    // view: the next press re-engages the pan instead of being inert.
    CHECK(viewer.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(viewer.pan_x() == 12);
    CHECK(!viewer.quit_requested());

    nmarkdown::Viewer wrapped;
    CHECK(load_markdown(wrapped, source));
    CHECK(wrapped.reader_state(0).code_wrap);
    CHECK(wrapped.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(!wrapped.dark_theme());
    CHECK(wrapped.handle_event({nmarkdown::InputEventType::PanRight, 0}));
    CHECK(wrapped.pan_x() == 12);
    CHECK(wrapped.handle_event({nmarkdown::InputEventType::Back, 0}));
    CHECK(wrapped.pan_x() == 0);
    CHECK(!wrapped.quit_requested());
}

void test_link_navigation_and_requests() {
    std::string source = "[Jump](#target)\n\n";
    for (int index = 0; index < 25; ++index) {
        source += "Paragraph filler before the target.\n\n";
    }
    source += "## Target\n\n"
              "[Next chapter](chapter2.md#part) and [Web](https://example.com).\n\n";
    for (int index = 0; index < 25; ++index) {
        source += "Paragraph filler after the target.\n\n";
    }
    auto document = std::unique_ptr<nmarkdown::MarkdownDocument>(
        new nmarkdown::MarkdownDocument());
    std::string error;
    CHECK(nmarkdown::parse_markdown(
        reinterpret_cast<const std::uint8_t*>(source.data()), source.size(),
        *document, error));
    nmarkdown::Viewer viewer;
    nmarkdown::DocumentProbe probe;
    probe.size = source.size();
    CHECK(viewer.set_markdown_document(std::move(document), probe, error));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.scroll_y() > 0);

    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSearch, 0}));
    for (char character : std::string("Next chapter")) {
        CHECK(viewer.handle_event({nmarkdown::InputEventType::TextInput, character}));
    }
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    // The block has two links, so Enter opens a visible chooser and a second
    // Enter activates the selected link instead of silently taking the first.
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    std::string target;
    CHECK(viewer.take_document_link_request(target));
    CHECK(target == "chapter2.md#part");
}

void test_multiple_links_are_keyboard_selectable() {
    const std::string source =
        "[First](one.md) [Second](two.md) [Third](three.md)\n";
    nmarkdown::Viewer viewer;
    CHECK(load_markdown(viewer, source));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::ScrollLineDown, 0}));
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Activate, 0}));
    std::string target;
    CHECK(viewer.take_document_link_request(target));
    CHECK(target == "two.md");
}

class MockDisplay final : public nmarkdown::Display {
public:
    bool initialize() override {
        pixels.assign(320 * 240, 0);
        initialized = true;
        return true;
    }
    void shutdown() override {
        shutdown_called = true;
        initialized = false;
    }
    nmarkdown::Surface565 surface() override {
        return initialized
                   ? nmarkdown::Surface565(pixels.data(), 320, 240, 320)
                   : nmarkdown::Surface565();
    }
    void present() override {
        ++present_count;
        const std::uint16_t accent = nmarkdown::rgb565(61, 144, 214);
        loading_frames.push_back(
            pixels[82U * 320U + 28U] == accent &&
            pixels[82U * 320U + 291U] == accent &&
            pixels[157U * 320U + 28U] == accent &&
            pixels[157U * 320U + 291U] == accent);
    }

    std::vector<std::uint16_t> pixels;
    int present_count = 0;
    std::vector<bool> loading_frames;
    bool initialized = false;
    bool shutdown_called = false;
};

class MockInput final : public nmarkdown::Input {
public:
    bool poll(nmarkdown::InputEvent& event) override {
        if (index >= events.size()) {
            return false;
        }
        event = events[index++];
        return true;
    }

    std::vector<nmarkdown::InputEvent> events{
        {nmarkdown::InputEventType::ScrollLineDown, 0},
        {nmarkdown::InputEventType::Back, 0},
    };
    std::size_t index = 0;
};

class MockClock final : public nmarkdown::Clock {
public:
    std::uint64_t milliseconds() const override { return now; }
    void sleep_ms(std::uint32_t duration) override { now += duration; }
    std::uint64_t now = 0;
};

class SlowFeedbackFileSystem final : public nmarkdown::FileSystem {
public:
    SlowFeedbackFileSystem(MockClock& clock, MockDisplay& display,
                           bool fail_read)
        : clock_(clock), display_(display), fail_read_(fail_read) {}

    bool probe(const char* path, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "slow.md") {
            error = "not found";
            return false;
        }
        result = {};
        result.size = source.size();
        return true;
    }

    bool read_all(const char* path, std::size_t,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "slow.md") {
            error = "not found";
            return false;
        }
        // Emulate a bounded synchronous read checkpoint after the no-flash
        // delay. The callback must present feedback before this method resumes
        // its remaining work.
        clock_.now = 200;
        report_operation_progress();
        feedback_seen_before_resume = !display_.loading_frames.empty() &&
                                      display_.loading_frames.back();
        if (fail_read_) {
            error = "simulated read failure";
            return false;
        }
        data.assign(source.begin(), source.end());
        return true;
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

    const std::string source =
        "# Slow document\n\nThe final reader frame replaces loading feedback.\n";
    bool feedback_seen_before_resume = false;

private:
    MockClock& clock_;
    MockDisplay& display_;
    bool fail_read_ = false;
};

class BusyDocumentBrowserFileSystem final : public nmarkdown::FileSystem {
public:
    explicit BusyDocumentBrowserFileSystem(MockDisplay& display)
        : display_(display) {}

    bool probe(const char* path, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "reader.md") {
            error = "not found";
            return false;
        }
        result = {};
        result.size = source.size();
        return true;
    }

    bool read_all(const char* path, std::size_t maximum_size,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "reader.md" ||
            source.size() > maximum_size) {
            error = "not found";
            return false;
        }
        data.assign(source.begin(), source.end());
        return true;
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

    bool list_reader_documents(const char*, std::size_t,
                               std::vector<std::string>& paths,
                               std::string& error,
                               bool* truncated) override {
        ++list_calls;
        feedback_seen_before_scan = !display_.loading_frames.empty() &&
                                    display_.loading_frames.back();
        const int presentations_before_progress = display_.present_count;
        // The logical clock deliberately remains unchanged. Long synchronous
        // scans must animate from bounded work checkpoints instead.
        for (int checkpoint = 0; checkpoint < 64; ++checkpoint) {
            report_operation_progress();
        }
        progress_presentations =
            display_.present_count - presentations_before_progress;
        paths = {"reader.md"};
        error.clear();
        if (truncated != nullptr) *truncated = false;
        return true;
    }

    const std::string source = "# Reader\n\nOpen Scratchpad to browse files.\n";
    int list_calls = 0;
    int progress_presentations = 0;
    bool feedback_seen_before_scan = false;

private:
    MockDisplay& display_;
};

class TnsWrappedMarkdownFileSystem final : public nmarkdown::FileSystem {
public:
    bool probe(const char* path,
               nmarkdown::DocumentProbe& result,
               std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "chapter.md.tns") {
            error = "not found";
            return false;
        }
        opened_wrapped_path = true;
        result = {};
        result.size = source.size();
        return true;
    }
    bool read_all(const char* path,
                  std::size_t maximum_size,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "chapter.md.tns" ||
            source.size() > maximum_size) {
            error = "not found";
            return false;
        }
        data.assign(source.begin(), source.end());
        return true;
    }
    bool read_range(const char* path,
                    std::uint64_t offset,
                    std::uint8_t* data,
                    std::size_t size,
                    std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "chapter.md.tns" ||
            offset > source.size() || size > source.size() - offset) {
            error = "invalid range";
            return false;
        }
        std::copy_n(source.data() + static_cast<std::size_t>(offset), size, data);
        return true;
    }
    bool write_atomic(const char*, const std::uint8_t*, std::size_t,
                      std::string&) override {
        return true;
    }

    const std::string source = "# Direct raw Markdown\n\nOpened without packing.\n";
    bool opened_wrapped_path = false;
};

class OversizedFontFileSystem final : public nmarkdown::FileSystem {
public:
    bool probe(const char* path, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested == "oversized.ttf") {
            result = {};
            result.size = 9U * 1024U * 1024U;
            return true;
        }
        if (requested == "oversized.ttf.tns") probed_retry = true;
        error = "could not open document: No such file or directory";
        return false;
    }
    bool read_all(const char* path, std::size_t,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        ++read_calls;
        data.clear();
        if (std::string(path == nullptr ? "" : path) != "oversized.ttf") {
            error = "could not open document: No such file or directory";
        } else {
            error = "file exceeds the configured size limit";
        }
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

    int read_calls = 0;
    bool probed_retry = false;
};

class BadAllocFontFileSystem final : public nmarkdown::FileSystem {
public:
    bool probe(const char* path, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "pressure.ttf") {
            error = "could not open document: No such file or directory";
            return false;
        }
        result = {};
        result.size = 4096;
        return true;
    }
    bool read_all(const char*, std::size_t,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        data.clear();
        error = "the streamed path should be attempted first";
        return false;
    }
    bool read_range(const char*, std::uint64_t, std::uint8_t*, std::size_t,
                    std::string& error) override {
        error = "unsupported";
        return false;
    }
    bool open_random_access(
        const char*, std::shared_ptr<nmarkdown::RandomAccessData>&,
        std::string&) override {
        ++open_calls;
        throw std::bad_alloc();
    }
    bool write_atomic(const char*, const std::uint8_t*, std::size_t,
                      std::string&) override {
        return true;
    }

    int open_calls = 0;
};

class AggregateFontLimitFileSystem final : public nmarkdown::FileSystem {
public:
    explicit AggregateFontLimitFileSystem(std::vector<std::uint8_t> font)
        : font_bytes(std::move(font)) {}

    bool probe(const char* path, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested != "body.ttf" && requested != "mono.ttf") {
            error = "could not open document: No such file or directory";
            return false;
        }
        result = {};
        result.size = font_bytes.size();
        return true;
    }
    bool read_all(const char* path, std::size_t maximum_size,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if ((requested != "body.ttf" && requested != "mono.ttf") ||
            font_bytes.size() > maximum_size) {
            error = "file exceeds the configured size limit";
            return false;
        }
        ++read_calls;
        data = font_bytes;
        return true;
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

    std::vector<std::uint8_t> font_bytes;
    int read_calls = 0;
};

class BinaryMarkdownFileSystem final : public nmarkdown::FileSystem {
public:
    bool probe(const char* path, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "binary.md") {
            error = "could not open document: No such file or directory";
            return false;
        }
        result = {};
        result.size = 8;
        return true;
    }
    bool read_all(const char* path, std::size_t,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "binary.md") {
            error = "could not open document: No such file or directory";
            return false;
        }
        data = {'O', 'T', 'T', 'O', 0, 1, 2, 3};
        return true;
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
};

class ExpandingUtf8FileSystem final : public nmarkdown::FileSystem {
public:
    bool probe(const char* path, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "malformed.md") {
            error = "could not open document: No such file or directory";
            return false;
        }
        result = {};
        result.size = 64;
        return true;
    }
    bool read_all(const char* path, std::size_t maximum_size,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        if (std::string(path == nullptr ? "" : path) != "malformed.md" ||
            maximum_size < 64) {
            error = "file exceeds the configured size limit";
            return false;
        }
        data.assign(64, 0xFFU);
        return true;
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
};

class CorruptStateFileSystem final : public nmarkdown::FileSystem {
public:
    bool probe(const char* path, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested == "state.md") {
            result = {};
            result.size = source.size();
            return true;
        }
        if (requested == "state.md.nmdstate") {
            result = {};
            result.size = 8;
            return true;
        }
        error = "could not open document: No such file or directory";
        return false;
    }
    bool read_all(const char* path, std::size_t,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested == "state.md") {
            data.assign(source.begin(), source.end());
            return true;
        }
        if (requested == "state.md.nmdstate") {
            data.assign(8, 0xA5);
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

    const std::string source = "# Saved state test\n\nReadable content.\n";
};

class FailingStateWriteFileSystem final : public nmarkdown::FileSystem {
public:
    bool probe(const char* path, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested == "write.md" || requested == "replacement.md") {
            result = {};
            result.size = requested == "write.md" ? source.size()
                                                    : replacement.size();
            return true;
        }
        error = "could not open document: No such file or directory";
        return false;
    }
    bool read_all(const char* path, std::size_t,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested == "write.md" || requested == "replacement.md") {
            const std::string& selected = requested == "write.md"
                                              ? source : replacement;
            data.assign(selected.begin(), selected.end());
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
                      std::string& error) override {
        ++write_attempts;
        error = "storage is read-only";
        return false;
    }
    bool list_reader_documents(const char*, std::size_t,
                               std::vector<std::string>& paths,
                               std::string& error,
                               bool* truncated) override {
        paths = {"replacement.md"};
        error.clear();
        if (truncated != nullptr) *truncated = false;
        return true;
    }

    const std::string source = "# State write test\n\nReadable content.\n";
    const std::string replacement =
        "# Replacement document\n\nThe prior state save fails.\n";
    int write_attempts = 0;
};

class BusyFontPickerFileSystem final : public nmarkdown::FileSystem {
public:
    explicit BusyFontPickerFileSystem(MockClock* slow_clock = nullptr)
        : slow_clock_(slow_clock) {}

    bool probe(const char*, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        ++probe_calls;
        return backing_.probe(metadata_font_path_.c_str(), result, error);
    }
    bool read_all(const char*, std::size_t,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        ++read_calls;
        data.clear();
        error = "font should not be read while browsing";
        return false;
    }
    bool read_range(const char*, std::uint64_t offset, std::uint8_t* data,
                    std::size_t size, std::string& error) override {
        ++range_calls;
        return backing_.read_range(metadata_font_path_.c_str(), offset, data,
                                   size, error);
    }
    bool write_atomic(const char*, const std::uint8_t*, std::size_t,
                      std::string&) override {
        return true;
    }
    bool list_font_files(const char*, std::size_t maximum_results,
                         std::vector<std::string>& paths,
                         std::string& error,
                         bool* truncated) override {
        ++list_calls;
        if (slow_clock_ != nullptr) {
            slow_clock_->now = 200;
            report_operation_progress();
        }
        paths.clear();
        error.clear();
        // Include the distribution filename plus a worst-case set of
        // duplicate basenames. Browsing must not open the multi-megabyte font.
        paths.emplace_back("/documents/SarasaFixedSC-Regular.ttf.tns");
        for (std::size_t index = 1; index < maximum_results; ++index) {
            paths.push_back("/documents/library-" + std::to_string(index) +
                            "/shared.ttf.tns");
        }
        if (truncated != nullptr) *truncated = true;
        return true;
    }

    int list_calls = 0;
    int probe_calls = 0;
    int range_calls = 0;
    int read_calls = 0;

private:
    MockClock* slow_clock_ = nullptr;
    nmarkdown::StdioFileSystem backing_;
    const std::string metadata_font_path_ =
        std::string(NMARKDOWN_SOURCE_DIR) + "/assets/fonts/DejaVuSans-CX.ttf";
};

class PersistentCjkFileSystem final : public nmarkdown::FileSystem {
public:
    explicit PersistentCjkFileSystem(std::vector<std::uint8_t> font)
        : font_bytes(std::move(font)) {}

    bool probe(const char* path, nmarkdown::DocumentProbe& result,
               std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested == "doc.md") {
            result = {};
            result.size = source.size();
            return true;
        }
        if (requested == font_path) {
            result = {};
            result.size = font_bytes.size();
            return true;
        }
        error = "could not open document: No such file or directory";
        return false;
    }

    bool read_all(const char* path, std::size_t maximum_size,
                  std::vector<std::uint8_t>& data,
                  std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested == "doc.md") {
            data.assign(source.begin(), source.end());
            return true;
        }
        if (requested == font_path && font_bytes.size() <= maximum_size) {
            data = font_bytes;
            ++font_read_calls;
            return true;
        }
        if (requested == preference_path && !preference_bytes.empty() &&
            preference_bytes.size() <= maximum_size) {
            data = preference_bytes;
            ++preference_read_calls;
            return true;
        }
        data.clear();
        error = "could not open document: No such file or directory";
        return false;
    }

    bool read_range(const char* path, std::uint64_t offset,
                    std::uint8_t* data, std::size_t size,
                    std::string& error) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested != font_path || offset > font_bytes.size() ||
            size > font_bytes.size() - static_cast<std::size_t>(offset)) {
            error = "font metadata range is unavailable";
            return false;
        }
        std::copy_n(font_bytes.data() + static_cast<std::size_t>(offset),
                    size, data);
        return true;
    }

    bool write_atomic(const char* path, const std::uint8_t* data,
                      std::size_t size, std::string&) override {
        const std::string requested(path == nullptr ? "" : path);
        if (requested == preference_path) {
            preference_bytes.assign(data, data + size);
            ++preference_write_calls;
        }
        return true;
    }

    bool list_font_files(const char*, std::size_t,
                         std::vector<std::string>& paths,
                         std::string& error, bool* truncated) override {
        paths = {font_path};
        error.clear();
        if (truncated != nullptr) *truncated = false;
        return true;
    }

    const std::string source = "# Font preference\n\nPlain body text.\n";
    const std::string font_path = "/documents/remembered-cjk.ttf.tns";
    const std::string preference_path =
        "/documents/.nmarkdown-fonts";
    std::vector<std::uint8_t> font_bytes;
    std::vector<std::uint8_t> preference_bytes;
    int font_read_calls = 0;
    int preference_read_calls = 0;
    int preference_write_calls = 0;
};

void test_application_lifecycle() {
    MockDisplay display;
    MockInput input;
    MockClock clock;
    nmarkdown::StdioFileSystem files;

    CHECK(nmarkdown::run_reader(display, input, files, clock, nullptr) == 0);
    CHECK(display.present_count == 2);
    CHECK(display.shutdown_called);
}

void test_ctrl_escape_quits_through_overlays() {
    nmarkdown::Viewer viewer;
    CHECK(viewer.handle_event({nmarkdown::InputEventType::OpenSettings, 0}));
    CHECK(!viewer.quit_requested());
    CHECK(viewer.handle_event({nmarkdown::InputEventType::Quit, 0}));
    CHECK(viewer.quit_requested());
}

void test_direct_tns_wrapped_markdown_open() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    TnsWrappedMarkdownFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = false;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "chapter.md", options) == 0);
    CHECK(files.opened_wrapped_path);
    // Launch is transactional: the first presented frame is the document,
    // never an intermediate "Opening document" surface.
    CHECK(display.present_count == 1);
    CHECK(std::find(display.loading_frames.begin(),
                    display.loading_frames.end(), true) ==
          display.loading_frames.end());
    CHECK(display.shutdown_called);
}

void test_slow_document_feedback_precedes_work_and_clears() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    SlowFeedbackFileSystem files(clock, display, false);
    nmarkdown::ReaderOptions options;
    options.persist_state = false;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "slow.md", options) == 0);
    CHECK(files.feedback_seen_before_resume);
    CHECK(std::find(display.loading_frames.begin(),
                    display.loading_frames.end(), true) !=
          display.loading_frames.end());
    CHECK(!display.loading_frames.empty());
    CHECK(!display.loading_frames.back());
}

void test_slow_document_error_clears_feedback() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    SlowFeedbackFileSystem files(clock, display, true);
    nmarkdown::ReaderOptions options;
    options.persist_state = false;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "slow.md", options) == 0);
    CHECK(files.feedback_seen_before_resume);
    CHECK(std::find(display.loading_frames.begin(),
                    display.loading_frames.end(), true) !=
          display.loading_frames.end());
    CHECK(!display.loading_frames.empty());
    CHECK(!display.loading_frames.back());
    const std::uint16_t failure = nmarkdown::rgb565(213, 76, 84);
    CHECK(std::count(display.pixels.begin(), display.pixels.end(), failure) > 5);
}

void test_scratchpad_browser_paints_immediately_and_reuses_catalog() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::OpenDocument, 0},
                    {nmarkdown::InputEventType::Back, 0},
                    {nmarkdown::InputEventType::OpenDocument, 0},
                    {nmarkdown::InputEventType::Back, 0},
                    {nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    BusyDocumentBrowserFileSystem files(display);
    nmarkdown::ReaderOptions options;
    options.persist_state = false;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "reader.md", options) == 0);
    CHECK(files.feedback_seen_before_scan);
    CHECK(files.progress_presentations == 8);
    CHECK(files.list_calls == 1);
    CHECK(std::count(display.loading_frames.begin(),
                     display.loading_frames.end(), true) >= 3);
    CHECK(!display.loading_frames.empty());
    CHECK(!display.loading_frames.back());
}

void test_font_size_error_is_not_overwritten_by_retry() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    OversizedFontFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = false;
    options.initial_body_font_path = "oversized.ttf";
    options.maximum_font_bytes = 8U * 1024U * 1024U;
    options.maximum_external_font_bytes = 8U * 1024U * 1024U;

    CHECK(nmarkdown::run_reader(display, input, files, clock, nullptr, options) == 0);
    // The probed size is rejected before allocating or reading the payload.
    CHECK(files.read_calls == 0);
    CHECK(!files.probed_retry);
    CHECK(std::find(display.loading_frames.begin(),
                    display.loading_frames.end(), true) ==
          display.loading_frames.end());
}

void test_font_bad_alloc_is_recoverable() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    BadAllocFontFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = false;
    options.initial_body_font_path = "pressure.ttf";

    CHECK(nmarkdown::run_reader(display, input, files, clock, nullptr,
                                options) == 0);
    CHECK(files.open_calls == 1);
    CHECK(display.shutdown_called);
    const std::uint16_t failure = nmarkdown::rgb565(213, 76, 84);
    CHECK(std::count(display.pixels.begin(), display.pixels.end(), failure) > 5);
}

void test_aggregate_external_font_limit_is_enforced_before_read() {
    nmarkdown::FontPack pack;
    std::string error;
    CHECK(pack.load_from_memory(nmarkdown::kCoreFontPack,
                                nmarkdown::kCoreFontPackSize, error));
    const nmarkdown::FontPackFace* face = pack.face(0);
    CHECK(face != nullptr);
    if (face == nullptr) return;
    std::vector<std::uint8_t> font(face->font_data,
                                   face->font_data + face->font_size);

    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    AggregateFontLimitFileSystem files(std::move(font));
    nmarkdown::ReaderOptions options;
    options.persist_state = false;
    options.initial_body_font_path = "body.ttf";
    options.initial_monospace_font_path = "mono.ttf";
    options.maximum_font_bytes = files.font_bytes.size() + 1;
    options.maximum_external_font_bytes = files.font_bytes.size() + 1;

    CHECK(nmarkdown::run_reader(display, input, files, clock, nullptr, options) == 0);
    CHECK(files.read_calls == 1);
}

void test_binary_direct_open_has_visible_error_state() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    BinaryMarkdownFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = false;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "binary.md", options) == 0);
    const std::uint16_t failure = nmarkdown::rgb565(213, 76, 84);
    CHECK(std::count(display.pixels.begin(), display.pixels.end(), failure) > 5);
}

void test_malformed_utf8_cannot_expand_past_source_budget() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    ExpandingUtf8FileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = false;
    options.maximum_source_bytes = 64;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "malformed.md", options) == 0);
    const std::uint16_t failure = nmarkdown::rgb565(213, 76, 84);
    CHECK(std::count(display.pixels.begin(), display.pixels.end(), failure) > 5);
}

void test_corrupt_state_is_reported_once() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Back, 0},
                    {nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    CorruptStateFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = true;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "state.md", options) == 0);
    // Document+warning, then the warning-dismiss repaint. No launch status
    // frame is presented.
    CHECK(display.present_count == 2);
}

void test_state_save_failure_is_visible_after_setting_change() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::OpenSettings, 0},
                    {nmarkdown::InputEventType::PanRight, 0},
                    {nmarkdown::InputEventType::Activate, 0},
                    {nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    FailingStateWriteFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = true;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "write.md", options) == 0);
    CHECK(files.write_attempts == 1);
    // Document, Settings, save warning, and dismiss repaint.
    CHECK(display.present_count == 4);
}

void test_state_save_failure_is_visible_before_normal_exit() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Back, 0},
                    {nmarkdown::InputEventType::Back, 0},
                    {nmarkdown::InputEventType::Quit, 0}};
    MockClock clock;
    FailingStateWriteFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = true;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "write.md", options) == 0);
    CHECK(files.write_attempts == 1);
    // The first Esc shows the warning; the second dismisses it and completes
    // the pending exit. The fallback Quit must remain unconsumed.
    CHECK(input.index == 2);
    CHECK(display.present_count == 2);
}

void test_state_save_failure_enter_dismissal_completes_exit() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Back, 0},
                    {nmarkdown::InputEventType::Activate, 0},
                    {nmarkdown::InputEventType::Quit, 0}};
    MockClock clock;
    FailingStateWriteFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = true;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "write.md", options) == 0);
    CHECK(files.write_attempts == 1);
    CHECK(input.index == 2);
    CHECK(display.present_count == 2);
}

void test_ctrl_escape_does_not_wait_for_state_warning() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::Quit, 0},
                    {nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    FailingStateWriteFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = true;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "write.md", options) == 0);
    CHECK(input.index == 1);
    // State is attempted during final shutdown, but Ctrl+Esc never waits for
    // an in-app warning dialog.
    CHECK(files.write_attempts == 1);
    CHECK(display.present_count == 1);
}

void test_document_replacement_save_failure_is_not_retried_on_exit() {
    MockDisplay display;
    MockInput input;
    input.events = {{nmarkdown::InputEventType::OpenDocument, 0},
                    {nmarkdown::InputEventType::Activate, 0},
                    {nmarkdown::InputEventType::Back, 0},
                    {nmarkdown::InputEventType::Back, 0}};
    MockClock clock;
    FailingStateWriteFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = true;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                "write.md", options) == 0);
    // Loading the replacement attempts the old document's state once. After
    // that warning is dismissed, exit must not repeat the known failure.
    CHECK(files.write_attempts == 1);
    CHECK(input.index == 4);
}

void test_cjk_picker_is_bounded_and_reuses_one_listing() {
    MockDisplay display;
    MockInput input;
    input.events.clear();
    input.events.push_back({nmarkdown::InputEventType::OpenSettings, 0});
    for (int row = 0; row < 12; ++row) {
        input.events.push_back({nmarkdown::InputEventType::ScrollLineDown, 0});
    }
    input.events.push_back({nmarkdown::InputEventType::Activate, 0});
    input.events.push_back({nmarkdown::InputEventType::ScrollLineDown, 0});
    input.events.push_back({nmarkdown::InputEventType::ScrollLineDown, 0});
    input.events.push_back({nmarkdown::InputEventType::Activate, 0});
    input.events.push_back({nmarkdown::InputEventType::PageDown, 0});
    input.events.push_back({nmarkdown::InputEventType::Back, 0});
    // Re-enter the CJK family. Its cached catalog is reused rather than
    // recursively scanning /documents again.
    input.events.push_back({nmarkdown::InputEventType::Activate, 0});
    input.events.push_back({nmarkdown::InputEventType::PageDown, 0});
    input.events.push_back({nmarkdown::InputEventType::Back, 0});
    input.events.push_back({nmarkdown::InputEventType::Back, 0});
    input.events.push_back({nmarkdown::InputEventType::Back, 0});

    MockClock clock;
    BusyFontPickerFileSystem files;
    nmarkdown::ReaderOptions options;
    options.persist_state = false;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                nullptr, options) == 0);
    CHECK(display.shutdown_called);
    CHECK(files.list_calls == 1);
    CHECK(files.probe_calls == 128);
    CHECK(files.range_calls > 0);
    CHECK(files.read_calls == 0);
    CHECK(std::find(display.loading_frames.begin(),
                    display.loading_frames.end(), true) ==
          display.loading_frames.end());
}

void test_slow_font_discovery_has_delayed_feedback() {
    MockDisplay display;
    MockInput input;
    input.events.clear();
    input.events.push_back({nmarkdown::InputEventType::OpenSettings, 0});
    for (int row = 0; row < 12; ++row) {
        input.events.push_back(
            {nmarkdown::InputEventType::ScrollLineDown, 0});
    }
    input.events.push_back({nmarkdown::InputEventType::Activate, 0});
    input.events.push_back({nmarkdown::InputEventType::Back, 0});
    input.events.push_back({nmarkdown::InputEventType::Back, 0});

    MockClock clock;
    BusyFontPickerFileSystem files(&clock);
    nmarkdown::ReaderOptions options;
    options.persist_state = false;

    CHECK(nmarkdown::run_reader(display, input, files, clock,
                                nullptr, options) == 0);
    CHECK(files.list_calls == 1);
    CHECK(std::find(display.loading_frames.begin(),
                    display.loading_frames.end(), true) !=
          display.loading_frames.end());
    CHECK(!display.loading_frames.empty());
    CHECK(!display.loading_frames.back());
}

void test_selected_cjk_font_is_remembered_across_runs() {
    nmarkdown::StdioFileSystem source_files;
    std::vector<std::uint8_t> cjk_font;
    std::string error;
    const std::string cjk_path = std::string(NMARKDOWN_BINARY_DIR) +
        "/fonts/SarasaFixedSC-Regular.ttf.tns";
    CHECK(source_files.read_all(cjk_path.c_str(), 12U * 1024U * 1024U,
                                cjk_font, error));
    if (cjk_font.empty()) return;
    PersistentCjkFileSystem files(std::move(cjk_font));

    nmarkdown::ReaderOptions options;
    options.document_root = "/documents";
    options.persist_state = true;

    MockDisplay first_display;
    MockInput first_input;
    first_input.events.clear();
    first_input.events.push_back({nmarkdown::InputEventType::OpenSettings, 0});
    for (int row = 0; row < 12; ++row) {
        first_input.events.push_back(
            {nmarkdown::InputEventType::ScrollLineDown, 0});
    }
    first_input.events.push_back({nmarkdown::InputEventType::Activate, 0});
    // Installed font -> CJK role -> back -> Apply changes.
    first_input.events.push_back({nmarkdown::InputEventType::Activate, 0});
    first_input.events.push_back({nmarkdown::InputEventType::ScrollLineDown, 0});
    first_input.events.push_back({nmarkdown::InputEventType::ScrollLineDown, 0});
    first_input.events.push_back({nmarkdown::InputEventType::ScrollLineDown, 0});
    first_input.events.push_back({nmarkdown::InputEventType::Activate, 0});
    first_input.events.push_back({nmarkdown::InputEventType::Back, 0});
    first_input.events.push_back({nmarkdown::InputEventType::ScrollLineDown, 0});
    first_input.events.push_back({nmarkdown::InputEventType::Activate, 0});
    first_input.events.push_back({nmarkdown::InputEventType::Quit, 0});
    MockClock first_clock;
    CHECK(nmarkdown::run_reader(first_display, first_input, files, first_clock,
                                "doc.md", options) == 0);
    CHECK(files.preference_write_calls == 1);
    CHECK(!files.preference_bytes.empty());
    CHECK(files.preference_bytes.size() >= 4);
    CHECK(files.preference_bytes[0] == 'N' &&
          files.preference_bytes[1] == 'M' &&
          files.preference_bytes[2] == 'F' &&
          files.preference_bytes[3] == '3');
    CHECK(files.font_read_calls == 1);
    CHECK(std::find(first_display.loading_frames.begin(),
                    first_display.loading_frames.end(), true) !=
          first_display.loading_frames.end());
    CHECK(!first_display.loading_frames.back());

    // A new application instance restores the selected path before opening
    // the document, without requiring the picker again.
    MockDisplay second_display;
    MockInput second_input;
    second_input.events = {{nmarkdown::InputEventType::Quit, 0}};
    MockClock second_clock;
    CHECK(nmarkdown::run_reader(second_display, second_input, files,
                                second_clock, "doc.md", options) == 0);
    CHECK(files.preference_read_calls == 1);
    CHECK(files.preference_write_calls == 1);
    CHECK(files.font_read_calls == 2);

    // Corruption is ignored safely and falls back to no external CJK face.
    files.preference_bytes.back() ^= 0x80U;
    MockDisplay third_display;
    MockInput third_input;
    third_input.events = {{nmarkdown::InputEventType::Quit, 0}};
    MockClock third_clock;
    CHECK(nmarkdown::run_reader(third_display, third_input, files, third_clock,
                                "doc.md", options) == 0);
    CHECK(files.font_read_calls == 2);
}

}  // namespace

int main() {
    test_surface_and_primitives();
    test_file_probe();
    test_document_browser();
    test_empty_document_has_an_explicit_state();
    test_startup_canvas_has_no_synthetic_document();
    test_loading_feedback_card_is_transient();
    test_retained_base_frame_accelerates_modal_repaints();
    test_jump_and_font_refresh_paint_the_first_frame();
    test_font_pack_menu();
    test_loaded_sarasa_cjk_picker_remains_responsive();
    test_viewer_scroll_and_render();
    test_progress_bar_tracks_continuous_position_in_both_modes();
    test_settings_theme_repaint_stays_responsive();
    test_layout_setting_reflow_waits_for_settings_close();
    test_decorated_blocks_use_exact_symmetric_outer_margins();
    test_modal_input_isolation_and_escape_order();
    test_toc_bookmark_empty_state_is_safe();
    test_vertical_and_horizontal_touchpad_modes();
    test_horizontal_scroll_restore_has_visible_content();
    test_horizontal_scroll_lazy_growth_reaches_document_end();
    test_markdown_toc_reflow_and_state();
    test_link_navigation_and_requests();
    test_multiple_links_are_keyboard_selectable();
    test_wide_block_focus();
    test_application_lifecycle();
    test_ctrl_escape_quits_through_overlays();
    test_direct_tns_wrapped_markdown_open();
    test_slow_document_feedback_precedes_work_and_clears();
    test_slow_document_error_clears_feedback();
    test_scratchpad_browser_paints_immediately_and_reuses_catalog();
    test_font_size_error_is_not_overwritten_by_retry();
    test_font_bad_alloc_is_recoverable();
    test_aggregate_external_font_limit_is_enforced_before_read();
    test_binary_direct_open_has_visible_error_state();
    test_malformed_utf8_cannot_expand_past_source_budget();
    test_corrupt_state_is_reported_once();
    test_state_save_failure_is_visible_after_setting_change();
    test_state_save_failure_is_visible_before_normal_exit();
    test_state_save_failure_enter_dismissal_completes_exit();
    test_ctrl_escape_does_not_wait_for_state_warning();
    test_document_replacement_save_failure_is_not_retried_on_exit();
    test_cjk_picker_is_bounded_and_reuses_one_listing();
    test_slow_font_discovery_has_delayed_feedback();
    test_selected_cjk_font_is_remembered_across_runs();

    if (failures != 0) {
        std::fprintf(stderr, "%d Phase 0 test(s) failed\n", failures);
        return 1;
    }
    std::printf("All Phase 0 tests passed\n");
    return 0;
}
