#include <cstdint>
#include <cstdio>

#include "clock_ndless.h"

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

}  // namespace

int main() {
    nmarkdown::ClockNdless clock;
    CHECK(clock.milliseconds() == 0);

    clock.sleep_ms(10);
    CHECK(clock.milliseconds() == 10);

    // The physical delay is capped, while logical time must still advance by
    // the complete request. This keeps an accidental large duration bounded.
    clock.sleep_ms(UINT32_C(1000000));
    CHECK(clock.milliseconds() == UINT64_C(1000010));

    if (failures != 0) {
        std::fprintf(stderr, "%d Ndless clock test(s) failed\n", failures);
        return 1;
    }
    std::puts("Ndless bounded clock tests passed");
    return 0;
}
