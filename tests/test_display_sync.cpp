#include <cstdint>
#include <cstdio>
#include <vector>

#include "nmarkdown/platform/ndless/display_sync.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,  \
                         __LINE__, #condition);                              \
            ++failures;                                                      \
        }                                                                    \
    } while (false)

void test_native_staging_matches_ndless_orientation() {
    constexpr int width = nmarkdown::display_sync::kLandscapeWidth;
    constexpr int height = nmarkdown::display_sync::kLandscapeHeight;
    constexpr int native_width = nmarkdown::display_sync::kNativeWidth;
    std::vector<std::uint16_t> landscape(
        static_cast<std::size_t>(width) * height);
    std::vector<std::uint16_t> native(landscape.size(), 0);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            landscape[static_cast<std::size_t>(y) * width + x] =
                static_cast<std::uint16_t>((y * 397 + x * 13) & 0xffff);
        }
    }

    nmarkdown::display_sync::stage_landscape_rgb565_for_native(
        landscape.data(), native.data());

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            CHECK(native[static_cast<std::size_t>(x) * native_width + y] ==
                  landscape[static_cast<std::size_t>(y) * width + x]);
        }
    }
}

void test_vertical_compare_requires_a_fresh_edge() {
    using nmarkdown::display_sync::VerticalCompareAction;
    using nmarkdown::display_sync::VerticalCompareEdge;

    // Starting while idle accepts the next assertion, never the idle sample.
    VerticalCompareEdge idle_start(true);
    CHECK(idle_start.observe(false) == VerticalCompareAction::Wait);
    CHECK(idle_start.observe(true) == VerticalCompareAction::Present);

    // A bit asserted at entry is stale. It can remain latched across several
    // reads and must be acknowledged until idle is observed; only a later
    // assertion is the next frame boundary.
    VerticalCompareEdge asserted_start(false);
    CHECK(asserted_start.observe(true) ==
          VerticalCompareAction::AcknowledgeCurrent);
    CHECK(asserted_start.observe(true) ==
          VerticalCompareAction::AcknowledgeCurrent);
    CHECK(asserted_start.observe(false) == VerticalCompareAction::Wait);
    CHECK(asserted_start.observe(false) == VerticalCompareAction::Wait);
    CHECK(asserted_start.observe(true) == VerticalCompareAction::Present);
}

}  // namespace

int main() {
    test_native_staging_matches_ndless_orientation();
    test_vertical_compare_requires_a_fresh_edge();

    if (failures != 0) {
        std::fprintf(stderr, "%d display synchronization test(s) failed\n",
                     failures);
        return 1;
    }
    std::puts("Display synchronization tests passed");
    return 0;
}
