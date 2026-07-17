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
    AwaitInitialEnterRelease,
    AwaitEnter,
    EnterSeen,
    RightSeen,
    SwipeOriginSeen,
    SwipeSeen,
    PostSwipeLeftSeen,
    FinalRightSeen,
};

Stage stage = Stage::AwaitInitialEnterRelease;
int swipe_origin_x = 0;

void trace(const char* marker) {
    std::printf("NMARKDOWN_IT/1 %s\n", marker);
    std::fflush(stdout);
}

void observe_arrow(int arrow, bool pressed) {
    if (!pressed) return;
    if (stage == Stage::EnterSeen && arrow == TPAD_ARROW_RIGHT) {
        stage = Stage::RightSeen;
        trace("OVERSIZED_FORMULA_RIGHT_INPUT");
    } else if (stage == Stage::SwipeSeen && arrow == TPAD_ARROW_LEFT) {
        stage = Stage::PostSwipeLeftSeen;
        trace("OVERSIZED_FORMULA_POST_SWIPE_LEFT_INPUT");
    } else if (stage == Stage::PostSwipeLeftSeen &&
               arrow == TPAD_ARROW_RIGHT) {
        stage = Stage::FinalRightSeen;
        trace("OVERSIZED_FORMULA_FINAL_RIGHT_INPUT");
    }
}

}  // namespace

// Linker wrapping observes the physical input after it reaches Ndless.  The
// verifier then requires a render/present cycle after each marker, proving the
// focused formula consumed both fine and page-sized pan input without hanging.
extern "C" BOOL __wrap_isKeyPressed(const t_key* key) {
    const BOOL pressed = __real_isKeyPressed(key);
    if (same_key(key, KEY_NSPIRE_ENTER)) {
        if (stage == Stage::AwaitInitialEnterRelease && !pressed) {
            // Ignore a possible launch-key hold inherited from TI-OS.
            stage = Stage::AwaitEnter;
        } else if (stage == Stage::AwaitEnter && pressed) {
            stage = Stage::EnterSeen;
            trace("OVERSIZED_FORMULA_ENTER_INPUT");
        }
    }
    if (same_key(key, KEY_NSPIRE_RIGHT)) {
        observe_arrow(TPAD_ARROW_RIGHT, pressed != 0);
    } else if (same_key(key, KEY_NSPIRE_LEFT)) {
        observe_arrow(TPAD_ARROW_LEFT, pressed != 0);
    }
    return pressed;
}

extern "C" int __wrap_touchpad_scan(touchpad_report_t* report) {
    const int result = __real_touchpad_scan(report);
    if (result != 0 || report == nullptr) return result;

    if (report->pressed) {
        observe_arrow(report->arrow, true);
        return result;
    }

    if (!report->contact) return result;
    if (stage == Stage::RightSeen) {
        swipe_origin_x = report->x;
        stage = Stage::SwipeOriginSeen;
    } else if (stage == Stage::SwipeOriginSeen &&
               report->x + 8 < swipe_origin_x) {
        stage = Stage::SwipeSeen;
        trace("OVERSIZED_FORMULA_SWIPE_LEFT_INPUT");
    }
    return result;
}
