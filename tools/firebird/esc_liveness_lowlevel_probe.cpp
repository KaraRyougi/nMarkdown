#include <cstdio>

#include <libndls.h>

extern "C" BOOL __real_isKeyPressed(const t_key* key);
extern "C" int __real_touchpad_scan(touchpad_report_t* report);

namespace {

bool same_key(const t_key* left, const t_key& right) {
    return left != nullptr && left->row == right.row && left->col == right.col &&
           left->tpad_row == right.tpad_row &&
           left->tpad_col == right.tpad_col &&
           left->tpad_arrow == right.tpad_arrow;
}

enum class Stage {
    AwaitHintEsc,
    HintEscPressed,
    HintEscReleased,
    HintDownSeen,
    TocPressed,
    TocEscPressed,
    TocEscReleased,
    TocDownSeen,
};

Stage stage = Stage::AwaitHintEsc;

void trace(const char* marker) {
    std::printf("NMARKDOWN_IT/1 %s\n", marker);
    std::fflush(stdout);
}

void observe_down(bool down) {
    if (!down) return;
    if (stage == Stage::HintEscReleased) {
        stage = Stage::HintDownSeen;
        trace("TRACE_LOWLEVEL_POST_HINT_DOWN_SCAN");
    } else if (stage == Stage::TocEscReleased) {
        stage = Stage::TocDownSeen;
        trace("TRACE_LOWLEVEL_POST_TOC_DOWN_SCAN");
    }
}

}  // namespace

extern "C" BOOL __wrap_isKeyPressed(const t_key* key) {
    const BOOL pressed = __real_isKeyPressed(key);
    if (same_key(key, KEY_NSPIRE_DOC) && pressed &&
        stage == Stage::HintDownSeen) {
        stage = Stage::TocPressed;
        trace("TRACE_LOWLEVEL_TOC_PRESS");
    }
    if (same_key(key, KEY_NSPIRE_ESC)) {
        if (pressed && stage == Stage::AwaitHintEsc) {
            stage = Stage::HintEscPressed;
            trace("TRACE_LOWLEVEL_HINT_ESC_PRESS");
        } else if (!pressed && stage == Stage::HintEscPressed) {
            stage = Stage::HintEscReleased;
            trace("TRACE_LOWLEVEL_HINT_ESC_RELEASE");
        } else if (pressed && stage == Stage::TocPressed) {
            stage = Stage::TocEscPressed;
            trace("TRACE_LOWLEVEL_TOC_ESC_PRESS");
        } else if (!pressed && stage == Stage::TocEscPressed) {
            stage = Stage::TocEscReleased;
            trace("TRACE_LOWLEVEL_TOC_ESC_RELEASE");
        }
    }
    observe_down(pressed && same_key(key, KEY_NSPIRE_DOWN));
    return pressed;
}

extern "C" int __wrap_touchpad_scan(touchpad_report_t* report) {
    const int result = __real_touchpad_scan(report);
    observe_down(result == 0 && report != nullptr && report->pressed &&
                 report->arrow == TPAD_ARROW_DOWN);
    return result;
}
