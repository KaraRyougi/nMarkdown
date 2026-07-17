#include <cstdint>
#include <cstdio>
#include <vector>

#include "nmarkdown/layout/fenwick.h"

namespace {
int failures = 0;
#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n",             \
                         __FILE__, __LINE__, #condition);                      \
            ++failures;                                                        \
        }                                                                      \
    } while (false)
}

int main() {
    nmarkdown::FenwickHeights heights;
    heights.build({nmarkdown::fx_from_int(10),
                   nmarkdown::fx_from_int(20),
                   nmarkdown::fx_from_int(30)});
    CHECK(heights.size() == 3);
    CHECK(heights.prefix_sum(0) == 0);
    CHECK(heights.prefix_sum(2) == nmarkdown::fx_from_int(30));
    CHECK(heights.total() == nmarkdown::fx_from_int(60));
    CHECK(heights.lower_bound(0) == 0);
    CHECK(heights.lower_bound(nmarkdown::fx_from_int(9)) == 0);
    CHECK(heights.lower_bound(nmarkdown::fx_from_int(10)) == 1);
    CHECK(heights.lower_bound(nmarkdown::fx_from_int(59)) == 2);
    CHECK(heights.lower_bound(nmarkdown::fx_from_int(60)) == 3);
    heights.update(1, nmarkdown::fx_from_int(5));
    CHECK(heights.total() == nmarkdown::fx_from_int(45));
    CHECK(heights.lower_bound(nmarkdown::fx_from_int(14)) == 1);
    CHECK(heights.lower_bound(nmarkdown::fx_from_int(15)) == 2);
    if (failures != 0) return 1;
    std::printf("All Fenwick tests passed\n");
    return 0;
}

