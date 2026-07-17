#include <cstdio>

#include <libndls.h>

extern "C" int __real_touchpad_scan(touchpad_report_t* report);

namespace {

enum class Stage {
    AwaitSwipeOrigin,
    AwaitSwipeMove,
    AwaitSwipeRelease,
    AwaitClickContact,
    ClickInjected,
    Complete,
};

Stage stage = Stage::AwaitSwipeOrigin;
int swipe_origin_y = 0;

void trace(const char* marker) {
    std::printf("NMARKDOWN_IT/1 %s\n", marker);
    std::fflush(stdout);
}

}  // namespace

// PocketJS supplies persistent touch contacts but has no separate scripted
// center-click action. Observe the real vertical report unchanged, then turn
// the following center contact into one native TPAD_ARROW_CLICK report. The
// Settings regression verifies that both the swipe and this physical click are
// ignored; the following keyboard events perform the actual option change.
extern "C" int __wrap_touchpad_scan(touchpad_report_t* report) {
    const int result = __real_touchpad_scan(report);
    if (result != 0 || report == nullptr) return result;

    if (stage == Stage::AwaitSwipeOrigin && report->contact &&
        !report->pressed) {
        swipe_origin_y = report->y;
        stage = Stage::AwaitSwipeMove;
    } else if (stage == Stage::AwaitSwipeMove && report->contact &&
               !report->pressed && report->y > swipe_origin_y + 8) {
        stage = Stage::AwaitSwipeRelease;
        trace("THEME_MODAL_SWIPE_UP_INPUT");
    } else if (stage == Stage::AwaitSwipeRelease && !report->contact) {
        stage = Stage::AwaitClickContact;
    } else if (stage == Stage::AwaitClickContact && report->contact &&
               !report->pressed) {
        report->pressed = true;
        report->arrow = TPAD_ARROW_CLICK;
        stage = Stage::ClickInjected;
        trace("THEME_MODAL_CLICK_INPUT");
    } else if (stage == Stage::ClickInjected && !report->contact) {
        stage = Stage::Complete;
        trace("THEME_MODAL_CLICK_RELEASE");
    }
    return result;
}
