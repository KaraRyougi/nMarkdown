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

bool tab_seen = false;
bool enter_seen = false;
bool click_seen = false;
bool tab_down = false;
bool enter_down = false;
bool down_down = false;
bool number2_down = false;

void trace(const char* marker) {
    std::printf("NMARKDOWN_IT/1 %s\n", marker);
    std::fflush(stdout);
}

void observe_key(bool pressed, bool& was_down, bool enabled,
                 const char* marker) {
    if (pressed && !was_down && enabled) trace(marker);
    was_down = pressed;
}

}  // namespace

// This fixture-only probe observes the exact matrix reports consumed by the
// production InputNdless mapper. It never changes matrix-key state.
extern "C" BOOL __wrap_isKeyPressed(const t_key* key) {
    const BOOL result = __real_isKeyPressed(key);
    const bool pressed = result != 0;

    if (same_key(key, KEY_NSPIRE_TAB)) {
        if (pressed && !tab_down) {
            tab_seen = true;
            trace("KEYMAP_TAB_INPUT");
        }
        tab_down = pressed;
    } else if (same_key(key, KEY_NSPIRE_ENTER)) {
        if (pressed && !enter_down && tab_seen) {
            enter_seen = true;
            trace("KEYMAP_ENTER_INPUT");
        }
        enter_down = pressed;
    } else if (same_key(key, KEY_NSPIRE_DOWN)) {
        observe_key(pressed, down_down, tab_seen, "KEYMAP_DOWN_INPUT");
    } else if (same_key(key, KEY_NSPIRE_2)) {
        observe_key(pressed, number2_down, tab_seen, "KEYMAP_N2_INPUT");
    }

    return result;
}

extern "C" int __wrap_touchpad_scan(touchpad_report_t* report) {
    const int result = __real_touchpad_scan(report);
    if (result != 0 || report == nullptr) return result;

    // CX/CX-II directional actions arrive through the touchpad report rather
    // than the clickpad matrix constants. Observe the baseline Down edge in
    // the same native report that InputNdless consumes.
    if (report->pressed && report->arrow == TPAD_ARROW_DOWN) {
        if (!down_down && tab_seen) trace("KEYMAP_DOWN_INPUT");
        down_down = true;
    } else {
        down_down = false;
    }

    // PocketJS can hold a touch contact but has no separate touchpad-click
    // action. In this dedicated fixture only, turn the first center contact
    // after Enter into one native click report. InputNdless and Viewer then
    // process the same report shape delivered by a physical touchpad click.
    if (tab_seen && enter_seen && !click_seen && report->contact &&
        !report->pressed) {
        click_seen = true;
        report->pressed = true;
        report->arrow = TPAD_ARROW_CLICK;
        trace("KEYMAP_CLICK_INPUT");
    }
    return result;
}
