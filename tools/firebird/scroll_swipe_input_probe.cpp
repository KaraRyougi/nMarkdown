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
    AwaitBaseDown,
    AwaitLeftOrigin,
    AwaitLeftMove,
    AwaitLeftRelease,
    AwaitRightOrigin,
    AwaitRightMove,
    AwaitRightRelease,
    AwaitLivenessDown,
    AwaitVerticalUpOrigin,
    AwaitVerticalUpMove,
    AwaitVerticalUpRelease,
    AwaitVerticalDownOrigin,
    AwaitVerticalDownMove,
    AwaitVerticalDownRelease,
    Complete,
};

Stage stage = Stage::AwaitBaseDown;
int swipe_origin_x = 0;
int swipe_origin_y = 0;

void trace(const char* marker) {
    std::printf("NMARKDOWN_IT/1 %s\n", marker);
    std::fflush(stdout);
}

void observe_arrow(int arrow, bool pressed) {
    if (!pressed || arrow != TPAD_ARROW_DOWN) return;
    if (stage == Stage::AwaitBaseDown) {
        stage = Stage::AwaitLeftOrigin;
        trace("SCROLL_SWIPE_BASE_DOWN_INPUT");
    } else if (stage == Stage::AwaitLivenessDown) {
        stage = Stage::AwaitVerticalUpOrigin;
        trace("SCROLL_SWIPE_POST_SWIPE_DOWN_INPUT");
    }
}

}  // namespace

// The probe observes actual Ndless input reports without changing them. Its
// markers let verify.mjs associate each physical direction with the next
// fixture-gated Viewer scroll delta and presentation.
extern "C" BOOL __wrap_isKeyPressed(const t_key* key) {
    const BOOL pressed = __real_isKeyPressed(key);
    if (same_key(key, KEY_NSPIRE_DOWN)) {
        observe_arrow(TPAD_ARROW_DOWN, pressed != 0);
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

    if (!report->contact) {
        if (stage == Stage::AwaitLeftRelease) {
            stage = Stage::AwaitRightOrigin;
        } else if (stage == Stage::AwaitRightRelease) {
            stage = Stage::AwaitLivenessDown;
        } else if (stage == Stage::AwaitVerticalUpRelease) {
            stage = Stage::AwaitVerticalDownOrigin;
        } else if (stage == Stage::AwaitVerticalDownRelease) {
            stage = Stage::Complete;
        }
        return result;
    }

    if (stage == Stage::AwaitLeftOrigin) {
        swipe_origin_x = report->x;
        stage = Stage::AwaitLeftMove;
    } else if (stage == Stage::AwaitLeftMove &&
               report->x + 8 < swipe_origin_x) {
        stage = Stage::AwaitLeftRelease;
        trace("SCROLL_SWIPE_LEFT_INPUT");
    } else if (stage == Stage::AwaitRightOrigin) {
        swipe_origin_x = report->x;
        stage = Stage::AwaitRightMove;
    } else if (stage == Stage::AwaitRightMove &&
               report->x > swipe_origin_x + 8) {
        stage = Stage::AwaitRightRelease;
        trace("SCROLL_SWIPE_RIGHT_INPUT");
    } else if (stage == Stage::AwaitVerticalUpOrigin) {
        swipe_origin_y = report->y;
        stage = Stage::AwaitVerticalUpMove;
    } else if (stage == Stage::AwaitVerticalUpMove &&
               report->y > swipe_origin_y + 8) {
        stage = Stage::AwaitVerticalUpRelease;
        trace("SCROLL_SWIPE_VERTICAL_UP_INPUT");
    } else if (stage == Stage::AwaitVerticalDownOrigin) {
        swipe_origin_y = report->y;
        stage = Stage::AwaitVerticalDownMove;
    } else if (stage == Stage::AwaitVerticalDownMove &&
               report->y + 8 < swipe_origin_y) {
        stage = Stage::AwaitVerticalDownRelease;
        trace("SCROLL_SWIPE_VERTICAL_DOWN_INPUT");
    }
    return result;
}
