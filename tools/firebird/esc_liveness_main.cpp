#include <libndls.h>

#include <cstdio>
#include <cstdint>

#include "clock_ndless.h"
#include "display_ndless.h"
#include "input_ndless.h"
#include "markdown_formula_fixture.h"
#include "nmarkdown/app/application.h"
#include "nmarkdown/platform/platform.h"

namespace {

constexpr char kFixturePath[] = "/documents/markdown-formula.md.tns";

void trace(const char* marker) {
    std::printf("NMARKDOWN_IT/1 %s\n", marker);
    std::fflush(stdout);
}

const char* event_name(nmarkdown::InputEventType type) {
    using nmarkdown::InputEventType;
    switch (type) {
    case InputEventType::None: return "none";
    case InputEventType::ScrollLineUp: return "line_up";
    case InputEventType::ScrollLineDown: return "line_down";
    case InputEventType::PageUp: return "page_up";
    case InputEventType::PageDown: return "page_down";
    case InputEventType::SwipeLeft: return "swipe_left";
    case InputEventType::SwipeRight: return "swipe_right";
    case InputEventType::SwipeUp: return "swipe_up";
    case InputEventType::SwipeDown: return "swipe_down";
    case InputEventType::PanLeft: return "pan_left";
    case InputEventType::PanRight: return "pan_right";
    case InputEventType::Activate: return "activate";
    case InputEventType::OpenMenu: return "open_menu";
    case InputEventType::OpenSearch: return "open_search";
    case InputEventType::SearchNext: return "search_next";
    case InputEventType::OpenSettings: return "open_settings";
    case InputEventType::OpenDiagnostics: return "open_diagnostics";
    case InputEventType::OpenDocument: return "open_document";
    case InputEventType::ToggleBookmark: return "toggle_bookmark";
    case InputEventType::IncreaseFont: return "increase_font";
    case InputEventType::DecreaseFont: return "decrease_font";
    case InputEventType::PointerScroll: return "pointer_scroll";
    case InputEventType::PointerPan: return "pointer_pan";
    case InputEventType::TextInput: return "text_input";
    case InputEventType::Backspace: return "backspace";
    case InputEventType::Back: return "back";
    case InputEventType::Quit: return "quit";
    }
    return "unknown";
}

enum class TraceStage : std::uint8_t {
    Startup,
    AfterHintEsc,
    AfterHintDown,
    TocOpen,
    AfterTocEsc,
    AfterTocDown,
    Quitting,
};

const char* stage_name(TraceStage stage) {
    switch (stage) {
    case TraceStage::Startup: return "startup";
    case TraceStage::AfterHintEsc: return "after_hint_esc";
    case TraceStage::AfterHintDown: return "after_hint_down";
    case TraceStage::TocOpen: return "toc_open";
    case TraceStage::AfterTocEsc: return "after_toc_esc";
    case TraceStage::AfterTocDown: return "after_toc_down";
    case TraceStage::Quitting: return "quitting";
    }
    return "unknown";
}

struct TraceState {
    TraceStage stage = TraceStage::Startup;
    unsigned poll_sequence = 0;
    unsigned poll_trace_budget = 0;
    unsigned sleep_sequence = 0;
    unsigned sleep_trace_budget = 0;

    void arm(unsigned count = 12) {
        poll_sequence = 0;
        sleep_sequence = 0;
        poll_trace_budget = count;
        sleep_trace_budget = count;
    }
};

class TracingClock final : public nmarkdown::Clock {
public:
    TracingClock(nmarkdown::Clock& inner, TraceState& state)
        : inner_(inner), state_(state) {}

    std::uint64_t milliseconds() const override {
        return inner_.milliseconds();
    }

    void sleep_ms(std::uint32_t duration) override {
        const bool traced = state_.sleep_trace_budget != 0;
        const unsigned sequence = ++state_.sleep_sequence;
        if (traced) {
            std::printf("NMARKDOWN_IT/1 TRACE_SLEEP_ENTER stage=%s seq=%u ms=%u\n",
                        stage_name(state_.stage), sequence,
                        static_cast<unsigned>(duration));
            std::fflush(stdout);
        }
        inner_.sleep_ms(duration);
        if (traced) {
            std::printf("NMARKDOWN_IT/1 TRACE_SLEEP_RETURN stage=%s seq=%u\n",
                        stage_name(state_.stage), sequence);
            std::fflush(stdout);
            --state_.sleep_trace_budget;
        }
    }

private:
    nmarkdown::Clock& inner_;
    TraceState& state_;
};

class TracingInput final : public nmarkdown::Input {
public:
    TracingInput(nmarkdown::Input& inner, TraceState& state)
        : inner_(inner), state_(state) {}

    bool poll(nmarkdown::InputEvent& event) override {
        const bool traced = state_.poll_trace_budget != 0;
        const unsigned sequence = ++state_.poll_sequence;
        if (traced) {
            std::printf("NMARKDOWN_IT/1 TRACE_POLL_ENTER stage=%s seq=%u\n",
                        stage_name(state_.stage), sequence);
            std::fflush(stdout);
        }

        const bool has_event = inner_.poll(event);
        if (traced) {
            std::printf(
                "NMARKDOWN_IT/1 TRACE_POLL_RETURN stage=%s seq=%u event=%s\n",
                stage_name(state_.stage), sequence,
                has_event ? event_name(event.type) : "idle");
            std::fflush(stdout);
            --state_.poll_trace_budget;
        }

        if (!has_event) return false;
        if (event.type == nmarkdown::InputEventType::Back &&
            state_.stage == TraceStage::Startup) {
            state_.stage = TraceStage::AfterHintEsc;
            state_.arm();
            trace("TRACE_CJK_HINT_ESC_EVENT");
        } else if (event.type == nmarkdown::InputEventType::ScrollLineDown &&
                   state_.stage == TraceStage::AfterHintEsc) {
            state_.stage = TraceStage::AfterHintDown;
            state_.arm();
            trace("TRACE_POST_HINT_DOWN_EVENT");
        } else if (event.type == nmarkdown::InputEventType::OpenMenu &&
                   state_.stage == TraceStage::AfterHintDown) {
            state_.stage = TraceStage::TocOpen;
            state_.arm();
            trace("TRACE_TOC_OPEN_EVENT");
        } else if (event.type == nmarkdown::InputEventType::Back &&
                   state_.stage == TraceStage::TocOpen) {
            state_.stage = TraceStage::AfterTocEsc;
            state_.arm();
            trace("TRACE_TOC_ESC_EVENT");
        } else if (event.type == nmarkdown::InputEventType::ScrollLineDown &&
                   state_.stage == TraceStage::AfterTocEsc) {
            state_.stage = TraceStage::AfterTocDown;
            state_.arm();
            trace("TRACE_POST_TOC_DOWN_EVENT");
        } else if (event.type == nmarkdown::InputEventType::Quit) {
            state_.stage = TraceStage::Quitting;
            state_.arm();
            trace("TRACE_CTRL_ESC_QUIT_EVENT");
        }
        return true;
    }

private:
    nmarkdown::Input& inner_;
    TraceState& state_;
};

class TracingDisplay final : public nmarkdown::Display {
public:
    TracingDisplay(nmarkdown::Display& inner, TraceState& state)
        : inner_(inner), state_(state) {}

    bool initialize() override { return inner_.initialize(); }
    void shutdown() override {
        trace("TRACE_DISPLAY_SHUTDOWN_ENTER");
        inner_.shutdown();
        trace("TRACE_DISPLAY_SHUTDOWN_RETURN");
    }

    nmarkdown::Surface565 surface() override {
        const bool traced = state_.stage != TraceStage::Startup;
        if (traced) trace("TRACE_SURFACE_ENTER");
        nmarkdown::Surface565 result = inner_.surface();
        if (traced) trace("TRACE_SURFACE_RETURN");
        return result;
    }

    void present() override {
        const bool traced = state_.stage != TraceStage::Startup;
        if (traced) trace("TRACE_PRESENT_ENTER");
        inner_.present();
        if (traced) trace("TRACE_PRESENT_RETURN");
    }

private:
    nmarkdown::Display& inner_;
    TraceState& state_;
};

bool write_exact_fixture() {
    FILE* file = std::fopen(kFixturePath, "wb");
    if (file == nullptr) return false;
    const bool wrote = std::fwrite(kMarkdownFormulaFixture, 1,
                                   kMarkdownFormulaFixtureSize, file) ==
                       kMarkdownFormulaFixtureSize;
    const bool closed = std::fclose(file) == 0;
    if (!wrote || !closed) return false;

    file = std::fopen(kFixturePath, "rb");
    if (file == nullptr) return false;
    std::uint64_t hash = UINT64_C(0xcbf29ce484222325);
    std::size_t bytes = 0;
    unsigned char buffer[512];
    for (;;) {
        const std::size_t read = std::fread(buffer, 1, sizeof(buffer), file);
        for (std::size_t index = 0; index < read; ++index) {
            hash ^= buffer[index];
            hash *= UINT64_C(0x100000001b3);
        }
        bytes += read;
        if (read != sizeof(buffer)) break;
    }
    const bool no_read_error = !std::ferror(file);
    const bool read_closed = std::fclose(file) == 0;
    const bool read_ok = no_read_error && read_closed;
    return read_ok && bytes == kMarkdownFormulaFixtureSize &&
           hash == kMarkdownFormulaFixtureFnv1a64;
}

}  // namespace

// The production main remains compiled unchanged. GNU ld redirects the Ndless
// startup reference to this fixture-only entry point with --wrap=main.
extern "C" int __wrap_main(int argc, char** argv) {
    trace("ENTER_MAIN");
    enable_relative_paths(argv);

    if (argc > 0 && argv[0] != nullptr) {
        cfg_register_fileext("md", "nmarkdown-firebird-esc-liveness");
        cfg_register_fileext("markdown", "nmarkdown-firebird-esc-liveness");
    }

    if (!write_exact_fixture()) {
        trace("FIXTURE_FAIL");
        return 2;
    }
    std::printf(
        "NMARKDOWN_IT/1 FIXTURE_EXACT bytes=%lu fnv1a64=%016llx\n",
        static_cast<unsigned long>(kMarkdownFormulaFixtureSize),
        static_cast<unsigned long long>(kMarkdownFormulaFixtureFnv1a64));
    std::printf("NMARKDOWN_IT/1 FIXTURE_SHA256 %s\n",
                kMarkdownFormulaFixtureSha256);
    std::fflush(stdout);
    trace("FIXTURE_READY");

    TraceState trace_state;
    nmarkdown::ClockNdless hardware_clock;
    TracingClock clock(hardware_clock, trace_state);
    nmarkdown::DisplayNdless hardware_display;
    TracingDisplay display(hardware_display, trace_state);
    nmarkdown::InputNdless hardware_input(clock);
    TracingInput input(hardware_input, trace_state);
    nmarkdown::StdioFileSystem files;

    nmarkdown::ReaderOptions options;
    options.maximum_source_bytes = is_cx2 ? 8U * 1024U * 1024U
                                           : 4U * 1024U * 1024U;
    options.maximum_font_bytes = is_cx2 ? 12U * 1024U * 1024U
                                         : 6U * 1024U * 1024U;
    options.maximum_external_font_bytes = options.maximum_font_bytes;
    options.persist_state = false;
    options.open_browser_on_empty_path = false;
    options.document_root = get_documents_dir();

    const int result = nmarkdown::run_reader(display, input, files, clock,
                                             kFixturePath, options);
    trace("TRACE_RUN_READER_RETURN");
    trace(result == 0 ? "EXIT_OK" : "EXIT_ERROR");
    return result;
}
