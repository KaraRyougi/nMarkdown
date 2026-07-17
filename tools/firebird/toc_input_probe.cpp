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

int doc_presses = 0;
bool doc_down = false;
bool final_escape_seen = false;
bool final_escape_released = false;
bool post_escape_down_reported = false;

void report_post_escape_down() {
    std::printf("NMARKDOWN_IT/1 TOC_POST_ESC_DOWN_INPUT\n");
    std::fflush(stdout);
}

void maybe_report_post_escape_down(bool down) {
    if (!post_escape_down_reported && final_escape_released && down) {
        post_escape_down_reported = true;
        report_post_escape_down();
    }
}

}  // namespace

// This linker-wrapped Ndless call is compiled only into the Firebird TOC
// fixture. It observes the actual keypad scan made by InputNdless, so the
// verifier can require a render/present sequence after the post-Esc Down key
// was consumed rather than accepting an unrelated global frame count.
extern "C" BOOL __wrap_isKeyPressed(const t_key* key) {
    const BOOL pressed = __real_isKeyPressed(key);

    if (same_key(key, KEY_NSPIRE_DOC)) {
        if (pressed && !doc_down) ++doc_presses;
        doc_down = pressed != 0;
    }

    if (doc_presses >= 4 && same_key(key, KEY_NSPIRE_ESC)) {
        if (pressed) {
            final_escape_seen = true;
        } else if (final_escape_seen) {
            final_escape_released = true;
        }
    }

    maybe_report_post_escape_down(pressed && same_key(key, KEY_NSPIRE_DOWN));

    return pressed;
}


extern "C" int __wrap_touchpad_scan(touchpad_report_t* report) {
    const int result = __real_touchpad_scan(report);
    maybe_report_post_escape_down(
        result == 0 && report != nullptr && report->pressed &&
        report->arrow == TPAD_ARROW_DOWN);
    return result;
}
