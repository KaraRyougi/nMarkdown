#include <cstdint>
#include <cstdio>
#include <unordered_set>

#include <libndls.h>

#include "input_ndless.h"

namespace {

int failures = 0;
std::unordered_set<int> pressed_keys;
touchpad_report_t touch_report{};
touchpad_info_t touch_info{100, 100};
int touchpad_scan_calls = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, line,
                 expression);
    ++failures;
}

#define CHECK(condition) check((condition), #condition, __LINE__)

class FakeClock final : public nmarkdown::Clock {
public:
    std::uint64_t milliseconds() const override { return now; }
    void sleep_ms(std::uint32_t duration) override { now += duration; }

    std::uint64_t now = 0;
};

void release_all() {
    pressed_keys.clear();
    touch_report = {};
}

void press(const t_key& key) {
    pressed_keys.insert(key.id);
}

void press_touchpad_arrow(tpad_arrow_t arrow) {
    touch_report.contact = 1;
    touch_report.pressed = 1;
    touch_report.arrow = static_cast<unsigned char>(arrow);
}

void expect_event(nmarkdown::InputNdless& input,
                  nmarkdown::InputEventType expected) {
    nmarkdown::InputEvent event;
    CHECK(input.poll(event));
    CHECK(event.type == expected);
}

void expect_event(nmarkdown::InputNdless& input,
                  nmarkdown::InputEventType expected,
                  int expected_amount,
                  nmarkdown::InputEventOrigin expected_origin =
                      nmarkdown::InputEventOrigin::Semantic) {
    nmarkdown::InputEvent event;
    CHECK(input.poll(event));
    CHECK(event.type == expected);
    CHECK(event.amount == expected_amount);
    CHECK(event.origin == expected_origin);
}

void test_escape_release_allows_later_navigation() {
    release_all();
    FakeClock clock;
    nmarkdown::InputNdless input(clock);

    press(KEY_NSPIRE_ESC);
    expect_event(input, nmarkdown::InputEventType::Back);
    CHECK(input.interaction_active());

    // A held non-repeatable Esc must not emit another event.
    nmarkdown::InputEvent event;
    CHECK(!input.poll(event));
    CHECK(input.interaction_active());

    // The harness leaves a full idle poll between releasing Esc and pressing
    // Down, matching the Firebird TOC regression tape.
    release_all();
    CHECK(!input.poll(event));
    CHECK(!input.interaction_active());
    press_touchpad_arrow(TPAD_ARROW_DOWN);
    expect_event(input, nmarkdown::InputEventType::ScrollLineDown);
}

void test_direct_escape_to_navigation_transition_stays_live() {
    release_all();
    FakeClock clock;
    nmarkdown::InputNdless input(clock);

    press(KEY_NSPIRE_ESC);
    expect_event(input, nmarkdown::InputEventType::Back);

    // Even without an intervening all-released poll, a new physical key must
    // replace Esc as the active key and produce its semantic event.
    release_all();
    press_touchpad_arrow(TPAD_ARROW_DOWN);
    expect_event(input, nmarkdown::InputEventType::ScrollLineDown);
}

void test_ctrl_escape_can_replace_held_escape() {
    release_all();
    FakeClock clock;
    nmarkdown::InputNdless input(clock);

    press(KEY_NSPIRE_ESC);
    expect_event(input, nmarkdown::InputEventType::Back);

    // Escape remains the physical key, but adding Ctrl changes its semantic
    // event. The input layer must therefore emit Quit immediately.
    press(KEY_NSPIRE_CTRL);
    expect_event(input, nmarkdown::InputEventType::Quit);
}

void test_idle_poll_uses_one_touchpad_snapshot() {
    release_all();
    touchpad_scan_calls = 0;
    FakeClock clock;
    nmarkdown::InputNdless input(clock);

    nmarkdown::InputEvent event;
    CHECK(!input.poll(event));
    CHECK(touchpad_scan_calls == 1);

    press_touchpad_arrow(TPAD_ARROW_DOWN);
    expect_event(input, nmarkdown::InputEventType::ScrollLineDown);
    CHECK(touchpad_scan_calls == 2);
}

void test_catalog_key_opens_bookmarks() {
    release_all();
    FakeClock clock;
    nmarkdown::InputNdless input(clock);
    press(KEY_NSPIRE_CAT);
    expect_event(input, nmarkdown::InputEventType::OpenBookmarks);
}

void test_production_menu_and_document_shortcuts() {
    FakeClock clock;

    {
        release_all();
        nmarkdown::InputNdless input(clock);
        press(KEY_NSPIRE_DOC);
        expect_event(input, nmarkdown::InputEventType::OpenMenu);
    }
    {
        release_all();
        nmarkdown::InputNdless input(clock);
        press(KEY_NSPIRE_MENU);
        expect_event(input, nmarkdown::InputEventType::OpenSettings);
    }
    {
        release_all();
        nmarkdown::InputNdless input(clock);
        press(KEY_NSPIRE_CTRL);
        press(KEY_NSPIRE_O);
        expect_event(input, nmarkdown::InputEventType::OpenDocument);
    }
    {
        release_all();
        nmarkdown::InputNdless input(clock);
        press(KEY_NSPIRE_CTRL);
        press(KEY_NSPIRE_F);
        expect_event(input, nmarkdown::InputEventType::OpenSearch);
    }
    {
        release_all();
        nmarkdown::InputNdless input(clock);
        press(KEY_NSPIRE_CTRL);
        press(KEY_NSPIRE_T);
        expect_event(input, nmarkdown::InputEventType::OpenSettings);
    }
}

void test_reader_navigation_key_aliases() {
    FakeClock clock;
    const auto expect_key = [&clock](const t_key& key,
                                     nmarkdown::InputEventType expected) {
        release_all();
        nmarkdown::InputNdless input(clock);
        press(key);
        expect_event(input, expected);
    };

    expect_key(KEY_NSPIRE_TAB, nmarkdown::InputEventType::PageDown);
    expect_key(KEY_NSPIRE_SCRATCHPAD, nmarkdown::InputEventType::OpenDocument);

    const auto expect_alias = [&clock](const t_key& key,
                                       nmarkdown::InputEventType expected,
                                       int digit) {
        release_all();
        nmarkdown::InputNdless input(clock);
        press(key);
        expect_event(input, expected, digit,
                     nmarkdown::InputEventOrigin::NumericNavigationAlias);
    };
    expect_alias(KEY_NSPIRE_1, nmarkdown::InputEventType::PageDown, '1');
    expect_alias(KEY_NSPIRE_7, nmarkdown::InputEventType::PageUp, '7');
    expect_alias(KEY_NSPIRE_2, nmarkdown::InputEventType::ScrollLineDown, '2');
    expect_alias(KEY_NSPIRE_4, nmarkdown::InputEventType::PanLeft, '4');
    expect_alias(KEY_NSPIRE_6, nmarkdown::InputEventType::PanRight, '6');
    expect_alias(KEY_NSPIRE_8, nmarkdown::InputEventType::ScrollLineUp, '8');

    // Unassigned digits retain text-entry behavior.
    const t_key text_digits[] = {
        KEY_NSPIRE_0, KEY_NSPIRE_3, KEY_NSPIRE_5, KEY_NSPIRE_9,
    };
    const int text_values[] = {'0', '3', '5', '9'};
    for (std::size_t index = 0;
         index < sizeof(text_digits) / sizeof(text_digits[0]); ++index) {
        release_all();
        nmarkdown::InputNdless text_input(clock);
        press(text_digits[index]);
        expect_event(text_input, nmarkdown::InputEventType::TextInput,
                     text_values[index]);
    }

    // Direction aliases also inherit modified-arrow semantics.
    release_all();
    nmarkdown::InputNdless modified(clock);
    press(KEY_NSPIRE_SHIFT);
    press(KEY_NSPIRE_2);
    expect_event(modified, nmarkdown::InputEventType::PageDown, '2',
                 nmarkdown::InputEventOrigin::NumericNavigationAlias);

    release_all();
    nmarkdown::InputNdless modified_up(clock);
    press(KEY_NSPIRE_CTRL);
    press(KEY_NSPIRE_8);
    expect_event(modified_up, nmarkdown::InputEventType::PageUp, '8',
                 nmarkdown::InputEventOrigin::NumericNavigationAlias);
}

void test_backlight_chords_are_left_to_the_system() {
    release_all();
    FakeClock clock;
    nmarkdown::InputNdless input(clock);
    nmarkdown::InputEvent event;

    // Ctrl+Plus/Minus must not also resize reader text. InputNdless only
    // observes the key matrix, so returning no semantic event leaves the
    // native OS backlight shortcut intact.
    press(KEY_NSPIRE_CTRL);
    press(KEY_NSPIRE_PLUS);
    CHECK(!input.poll(event));

    release_all();
    CHECK(!input.poll(event));
    press(KEY_NSPIRE_PLUS);
    expect_event(input, nmarkdown::InputEventType::IncreaseFont);

    release_all();
    CHECK(!input.poll(event));
    press(KEY_NSPIRE_CTRL);
    press(KEY_NSPIRE_MINUS);
    CHECK(!input.poll(event));

    release_all();
    CHECK(!input.poll(event));
    press(KEY_NSPIRE_MINUS);
    expect_event(input, nmarkdown::InputEventType::DecreaseFont);
}

void test_page_alias_key_repeat() {
    release_all();
    FakeClock clock;
    nmarkdown::InputNdless input(clock);
    nmarkdown::InputEvent event;

    press(KEY_NSPIRE_1);
    expect_event(input, nmarkdown::InputEventType::PageDown, '1',
                 nmarkdown::InputEventOrigin::NumericNavigationAlias);
    clock.now = 359;
    CHECK(!input.poll(event));
    clock.now = 360;
    expect_event(input, nmarkdown::InputEventType::PageDown, '1',
                 nmarkdown::InputEventOrigin::NumericNavigationAlias);
}

void test_reader_alias_priority_and_direct_transitions() {
    release_all();
    FakeClock clock;
    nmarkdown::InputNdless input(clock);

    press(KEY_NSPIRE_TAB);
    expect_event(input, nmarkdown::InputEventType::PageDown);
    release_all();
    press(KEY_NSPIRE_1);
    expect_event(input, nmarkdown::InputEventType::PageDown);

    // Distinct physical identities must emit distinct edges even when there
    // is no all-released poll between equivalent aliases.
    release_all();
    press_touchpad_arrow(TPAD_ARROW_DOWN);
    expect_event(input, nmarkdown::InputEventType::ScrollLineDown);
    release_all();
    press(KEY_NSPIRE_2);
    expect_event(input, nmarkdown::InputEventType::ScrollLineDown);

    release_all();
    nmarkdown::InputNdless escape_priority(clock);
    press(KEY_NSPIRE_ESC);
    press(KEY_NSPIRE_1);
    expect_event(escape_priority, nmarkdown::InputEventType::Back);

    release_all();
    nmarkdown::InputNdless shortcut_priority(clock);
    press(KEY_NSPIRE_CTRL);
    press(KEY_NSPIRE_O);
    press(KEY_NSPIRE_2);
    expect_event(shortcut_priority, nmarkdown::InputEventType::OpenDocument);

    release_all();
    nmarkdown::InputNdless scratchpad(clock);
    nmarkdown::InputEvent event;
    clock.now = 0;
    press(KEY_NSPIRE_SCRATCHPAD);
    expect_event(scratchpad, nmarkdown::InputEventType::OpenDocument);
    clock.now = 1000;
    CHECK(!scratchpad.poll(event));
}

void test_touch_sampler_emits_discrete_and_continuous_axis_events() {
    release_all();
    FakeClock clock;
    nmarkdown::InputNdless input(clock);
    nmarkdown::InputEvent event;

    // Establish a persistent contact at the center. Gesture detection starts
    // on a later sample so a tap cannot be mistaken for a swipe.
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));
    CHECK(input.interaction_active());

    touch_report.x = 45;
    expect_event(input, nmarkdown::InputEventType::PointerPan, -12);
    touch_report.x = 20;
    expect_event(input, nmarkdown::InputEventType::SwipeLeft);
    expect_event(input, nmarkdown::InputEventType::PointerPan, -60);
    CHECK(!input.poll(event));  // One named swipe per uninterrupted contact.

    release_all();
    CHECK(!input.poll(event));
    CHECK(!input.interaction_active());
    touch_report.contact = 1;
    touch_report.x = 40;
    touch_report.y = 50;
    CHECK(!input.poll(event));

    touch_report.x = 45;
    expect_event(input, nmarkdown::InputEventType::PointerPan, 12);
    touch_report.x = 70;
    expect_event(input, nmarkdown::InputEventType::SwipeRight);
    expect_event(input, nmarkdown::InputEventType::PointerPan, 60);

    release_all();
    CHECK(!input.poll(event));
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));

    // The raw Ndless touchpad Y coordinate increases toward the physical top
    // of the pad (opposite to screen/Firebird GUI coordinates).  A physical
    // upward drag therefore increases report.y, but emits negative semantic
    // motion and SwipeUp.
    touch_report.y = 55;
    expect_event(input, nmarkdown::InputEventType::PointerScroll, -12);
    touch_report.y = 80;
    expect_event(input, nmarkdown::InputEventType::SwipeUp);
    expect_event(input, nmarkdown::InputEventType::PointerScroll, -60);
    CHECK(!input.poll(event));

    release_all();
    CHECK(!input.poll(event));
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));

    touch_report.y = 45;
    expect_event(input, nmarkdown::InputEventType::PointerScroll, 12);
    touch_report.y = 20;
    expect_event(input, nmarkdown::InputEventType::SwipeDown);
    expect_event(input, nmarkdown::InputEventType::PointerScroll, 60);
}

void test_touchpad_tap_activates_without_drag_or_duplicate_click() {
    release_all();
    FakeClock clock;
    nmarkdown::InputNdless input(clock);
    nmarkdown::InputEvent event;

    // A short contact with only one native-coordinate unit of jitter is a
    // tap-to-activate. The origin lets passive overlays distinguish it from
    // keyboard Enter without changing keyboard behavior.
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));
    clock.now = 100;
    touch_report.x = 51;
    touch_report.y = 49;
    CHECK(!input.poll(event));
    clock.now = 180;
    release_all();
    expect_event(input, nmarkdown::InputEventType::Activate, 0,
                 nmarkdown::InputEventOrigin::TouchpadTap);
    CHECK(!input.poll(event));

    // A held contact is not a click.
    clock.now = 1000;
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));
    clock.now = 1600;
    release_all();
    CHECK(!input.poll(event));

    // Any axis-locked drag suppresses the release activation.
    clock.now = 1700;
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));
    clock.now = 1740;
    touch_report.y = 55;
    expect_event(input, nmarkdown::InputEventType::PointerScroll, -12);
    clock.now = 1780;
    release_all();
    CHECK(!input.poll(event));

    // A diagonal move can stay outside the physical-scale 4:3 swipe-axis lock. It
    // still exceeds the tap slop and must not activate on release.
    clock.now = 1800;
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));
    clock.now = 1840;
    touch_report.x = 52;
    touch_report.y = 52;
    CHECK(!input.poll(event));
    clock.now = 1880;
    release_all();
    CHECK(!input.poll(event));

    // A physical center press is the same pointer activation origin. Its
    // release must not synthesize a second tap activation.
    clock.now = 2000;
    press_touchpad_arrow(TPAD_ARROW_CLICK);
    expect_event(input, nmarkdown::InputEventType::Activate, 0,
                 nmarkdown::InputEventOrigin::TouchpadActivation);
    // Button pressure may end before the finger leaves the pad. That residual
    // contact must not begin another tap candidate.
    touch_report.pressed = 0;
    touch_report.arrow = TPAD_ARROW_NONE;
    CHECK(!input.poll(event));
    release_all();
    CHECK(!input.poll(event));
}

void test_touchpad_deadzones_filter_jitter_and_lock_one_axis() {
    release_all();
    touch_info = {100, 100};
    FakeClock clock;
    nmarkdown::InputNdless input(clock);
    nmarkdown::InputEvent event;

    // Two separate deadzones are intentional: this landing wobble is too
    // large to be a tap, but too small to start a drag or steal the axis.
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));
    touch_report.x = 53;
    CHECK(!input.poll(event));
    touch_report.y = 42;
    expect_event(input, nmarkdown::InputEventType::PointerScroll, 19);
    release_all();
    CHECK(!input.poll(event));

    // The symmetric case must retain horizontal intent after an initial
    // sub-lock vertical wobble.
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));
    touch_report.y = 47;
    CHECK(!input.poll(event));
    touch_report.x = 58;
    expect_event(input, nmarkdown::InputEventType::PointerPan, 19);

    release_all();
    CHECK(!input.poll(event));

    // Equal diagonal travel well beyond the drag-start threshold remains in
    // the angular deadzone. It locks only after horizontal intent is clear.
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));
    touch_report.x = 56;
    touch_report.y = 44;
    CHECK(!input.poll(event));
    touch_report.x = 60;
    touch_report.y = 45;
    expect_event(input, nmarkdown::InputEventType::PointerPan, 24);

    release_all();
    CHECK(!input.poll(event));

    // An early horizontal landing wobble can be corrected once when later
    // travel is decisively vertical.
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));
    touch_report.x = 46;
    touch_report.y = 52;
    expect_event(input, nmarkdown::InputEventType::PointerPan, -9);
    touch_report.x = 45;
    touch_report.y = 25;
    expect_event(input, nmarkdown::InputEventType::SwipeDown);
    expect_event(input, nmarkdown::InputEventType::PointerScroll, 60);
    touch_report.x = 20;
    touch_report.y = 24;
    CHECK(!input.poll(event));  // One correction; it cannot flip back again.
    touch_report.y = 22;
    expect_event(input, nmarkdown::InputEventType::PointerScroll, 7);

    release_all();
    CHECK(!input.poll(event));

    // Symmetric landing wobble: an initially vertical lock must not swallow a
    // later clear left/right gesture or continuous horizontal movement.
    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));
    touch_report.y = 45;
    expect_event(input, nmarkdown::InputEventType::PointerScroll, 12);
    touch_report.x = 65;
    expect_event(input, nmarkdown::InputEventType::PointerPan, 36);
    touch_report.x = 75;
    expect_event(input, nmarkdown::InputEventType::SwipeRight);
    expect_event(input, nmarkdown::InputEventType::PointerPan, 24);
}

void test_touchpad_motion_deadzone_accumulates_slow_drag() {
    release_all();
    touch_info = {100, 100};
    FakeClock clock;
    nmarkdown::InputNdless input(clock);
    nmarkdown::InputEvent event;

    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));
    touch_report.x = 46;
    expect_event(input, nmarkdown::InputEventType::PointerPan, -9);

    // A one-unit sample is suppressed, but remains accumulated from the last
    // emitted coordinate; the next unit completes the motion deadzone.
    touch_report.x = 45;
    CHECK(!input.poll(event));
    touch_report.x = 44;
    expect_event(input, nmarkdown::InputEventType::PointerPan, -4);
}

void test_touchpad_axis_choice_preserves_rectangular_pad_angles() {
    release_all();
    touch_info = {2328, 1691};
    FakeClock clock;
    nmarkdown::InputNdless input(clock);
    nmarkdown::InputEvent event;

    touch_report.contact = 1;
    touch_report.x = 1164;
    touch_report.y = 845;
    CHECK(!input.poll(event));

    // Firebird's 100 x 70 physical model maps to 2328 x 1691 native units, so
    // a visual 10 x 6 move is about 233 x 145 native units. It is physically
    // horizontal; full-range normalization would incorrectly leave it stuck.
    touch_report.x = 1397;
    touch_report.y = 700;
    expect_event(input, nmarkdown::InputEventType::PointerPan, 24);
    release_all();
    CHECK(!input.poll(event));

    touch_report.contact = 1;
    touch_report.x = 1164;
    touch_report.y = 845;
    CHECK(!input.poll(event));

    // A diagonal 140 x 169-native-unit move is intentionally ambiguous.
    touch_report.x = 1304;
    touch_report.y = 676;
    CHECK(!input.poll(event));
    release_all();
    CHECK(!input.poll(event));

    touch_report.contact = 1;
    touch_report.x = 1164;
    touch_report.y = 845;
    CHECK(!input.poll(event));

    // Increasing vertical travel to 242 units makes intent unambiguous.
    touch_report.x = 1304;
    touch_report.y = 603;
    expect_event(input, nmarkdown::InputEventType::PointerScroll, 34);

    release_all();
    touch_info = {100, 100};
}

void test_touchpad_swipe_deadzone_boundary_emits_once() {
    release_all();
    touch_info = {100, 100};
    FakeClock clock;
    nmarkdown::InputNdless input(clock);
    nmarkdown::InputEvent event;

    touch_report.contact = 1;
    touch_report.x = 50;
    touch_report.y = 50;
    CHECK(!input.poll(event));

    // Nineteen percent remains a continuous drag; the 20% boundary produces
    // exactly one named swipe even though its final one-unit sample is inside
    // the continuous-motion deadzone.
    touch_report.x = 69;
    expect_event(input, nmarkdown::InputEventType::PointerPan, 45);
    touch_report.x = 70;
    expect_event(input, nmarkdown::InputEventType::SwipeRight);
    CHECK(!input.poll(event));

    // Reversing past the origin during the same contact cannot emit a second
    // named swipe in the opposite direction.
    touch_report.x = 30;
    expect_event(input, nmarkdown::InputEventType::PointerPan, -93);
    CHECK(!input.poll(event));
}

}  // namespace

BOOL nmarkdown_test_is_key_pressed(const t_key* key) {
    if (key != nullptr && key->tpad_arrow != TPAD_ARROW_NONE) {
        touchpad_report_t report{};
        if (touchpad_scan(&report) != 0 || !report.pressed) return 0;
        return report.arrow == key->tpad_arrow ? 1 : 0;
    }
    return key != nullptr && pressed_keys.count(key->id) != 0 ? 1 : 0;
}

BOOL _is_touchpad() {
    return 1;
}

int touchpad_scan(touchpad_report_t* report) {
    ++touchpad_scan_calls;
    if (report != nullptr) *report = touch_report;
    return 0;
}

touchpad_info_t* touchpad_getinfo() {
    return &touch_info;
}

int main() {
    test_escape_release_allows_later_navigation();
    test_direct_escape_to_navigation_transition_stays_live();
    test_ctrl_escape_can_replace_held_escape();
    test_idle_poll_uses_one_touchpad_snapshot();
    test_catalog_key_opens_bookmarks();
    test_production_menu_and_document_shortcuts();
    test_reader_navigation_key_aliases();
    test_backlight_chords_are_left_to_the_system();
    test_page_alias_key_repeat();
    test_reader_alias_priority_and_direct_transitions();
    test_touch_sampler_emits_discrete_and_continuous_axis_events();
    test_touchpad_tap_activates_without_drag_or_duplicate_click();
    test_touchpad_deadzones_filter_jitter_and_lock_one_axis();
    test_touchpad_motion_deadzone_accumulates_slow_drag();
    test_touchpad_axis_choice_preserves_rectangular_pad_angles();
    test_touchpad_swipe_deadzone_boundary_emits_once();

    if (failures != 0) {
        std::fprintf(stderr, "%d input test(s) failed\n", failures);
        return 1;
    }
    std::puts("Ndless input release/liveness tests passed");
    return 0;
}
