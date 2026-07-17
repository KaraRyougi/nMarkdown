#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "nmarkdown/layout/plain_text_layout.h"
#include "nmarkdown/text/text_system.h"

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

class CountingRandomAccess final : public nmarkdown::RandomAccessData {
public:
    struct Read {
        std::uint64_t offset = 0;
        std::size_t size = 0;
    };

    explicit CountingRandomAccess(std::string bytes)
        : bytes_(std::move(bytes)) {}

    std::uint64_t size() const override { return bytes_.size(); }

    bool read(std::uint64_t offset,
              std::uint8_t* data,
              std::size_t size) override {
        if (offset > bytes_.size() || size > bytes_.size() - offset) {
            return false;
        }
        if (size != 0) {
            std::memcpy(data, bytes_.data() + static_cast<std::size_t>(offset),
                        size);
        }
        bytes_read += size;
        largest_read = std::max(largest_read, size);
        ++read_calls;
        reads.push_back({offset, size});
        return true;
    }

    std::size_t bytes_read = 0;
    std::size_t largest_read = 0;
    std::size_t read_calls = 0;
    std::vector<Read> reads;

private:
    std::string bytes_;
};

class ContiguousCountingRandomAccess final
    : public nmarkdown::RandomAccessData {
public:
    explicit ContiguousCountingRandomAccess(std::string bytes)
        : bytes_(std::move(bytes)) {}

    std::uint64_t size() const override { return bytes_.size(); }
    const std::uint8_t* contiguous_data() const override {
        return reinterpret_cast<const std::uint8_t*>(bytes_.data());
    }

    bool read(std::uint64_t offset,
              std::uint8_t* data,
              std::size_t size) override {
        ++read_calls;
        if (offset > bytes_.size() || size > bytes_.size() - offset) {
            return false;
        }
        if (size != 0) {
            std::memcpy(data, bytes_.data() + static_cast<std::size_t>(offset),
                        size);
        }
        return true;
    }

    std::size_t read_calls = 0;

private:
    std::string bytes_;
};

std::string large_text() {
    constexpr std::size_t kFixtureBytes =
        1U * 1024U * 1024U + 192U * 1024U;
    std::string result;
    result.reserve(kFixtureBytes + 4096U);
    for (int line = 0;
         result.size() < kFixtureBytes;
         ++line) {
        result += "Line " + std::to_string(line) +
                  " keeps the TXT reader streaming without a global index. ";
        result += (line % 7 == 0) ? u8"中文排版测试\n" : "ASCII text\n";
    }
    result += "unique-streaming-search-needle\n";
    return result;
}

std::string sliding_cache_text() {
    constexpr std::size_t kFixtureBytes =
        nmarkdown::kPlainTextRawCacheBytes + 512U * 1024U;
    std::string result;
    result.reserve(kFixtureBytes + 128U);
    for (int line = 0; result.size() < kFixtureBytes; ++line) {
        result += "Sliding cache line " + std::to_string(line) +
                  " keeps sequential NAND reads off the input frame.\n";
    }
    return result;
}

void warm_ready_glyphs(nmarkdown::PlainTextLayout& layout) {
    int guard = 0;
    while (layout.preload_next_glyphs(64) != 0 &&
           guard < 4096) {
        ++guard;
    }
    CHECK(guard < 4096);
}

void test_first_screen_cache_and_idle_prefetch() {
    const std::string content = large_text();
    auto source = std::make_shared<CountingRandomAccess>(content);
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));

    nmarkdown::PlainTextLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(
        source, 0, static_cast<std::uint32_t>(content.size()), {}, text,
        signature, 220, error));
    CHECK(error.empty());
    CHECK(layout.raw_cache_size() ==
          nmarkdown::kPlainTextInitialCacheBytes);
    CHECK(source->bytes_read ==
          nmarkdown::kPlainTextInitialCacheBytes);
    CHECK(layout.cached_screen_count() >= 1);
    CHECK(layout.cached_screen_count() <=
          1 + nmarkdown::kPlainTextFutureScreens);
    CHECK(!layout.visible_lines(error).empty());
    CHECK(source->bytes_read ==
          nmarkdown::kPlainTextInitialCacheBytes);

    // A future screen is assembled one wrapped line per idle input poll. The
    // first visible screen does not wait for the complete source window.
    int first_future_guard = 0;
    while (layout.future_screen_count() == 0 &&
           first_future_guard < 64) {
        CHECK(layout.perform_incremental_work(error));
        CHECK(error.empty());
        ++first_future_guard;
    }
    CHECK(first_future_guard > 1);
    CHECK(first_future_guard < 64);
    CHECK(layout.future_screen_count() >= 1);
    const std::size_t after_layout = layout.raw_cache_size();
    CHECK(layout.perform_incremental_work(error));
    CHECK(error.empty());
    CHECK(layout.raw_cache_size() >= after_layout);

    const std::size_t expected_cache_size =
        std::min(content.size(), layout.raw_cache_capacity());
    int prefetch_guard = 0;
    const int prefetch_guard_limit = static_cast<int>(
        expected_cache_size / nmarkdown::kPlainTextPrefetchBytes + 512U);
    while (layout.raw_cache_size() < expected_cache_size &&
           prefetch_guard < prefetch_guard_limit) {
        layout.perform_incremental_work(error);
        CHECK(error.empty());
        ++prefetch_guard;
    }
    CHECK(prefetch_guard < prefetch_guard_limit);
    CHECK(layout.raw_cache_size() == expected_cache_size);
    CHECK(layout.future_screen_count() <= nmarkdown::kPlainTextFutureScreens);
    CHECK(layout.cached_screen_count() <=
          1 + nmarkdown::kPlainTextFutureScreens);
    CHECK(source->largest_read <= nmarkdown::kPlainTextInitialCacheBytes);
}

void test_contiguous_source_is_not_copied_or_reread() {
    const std::string content = large_text();
    auto source = std::make_shared<ContiguousCountingRandomAccess>(content);
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));

    nmarkdown::PlainTextLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(
        source, 0, static_cast<std::uint32_t>(content.size()), {}, text,
        signature, 220, error));
    CHECK(error.empty());
    CHECK(layout.raw_cache_size() == content.size());
    CHECK(source->read_calls == 0);

    CHECK(layout.prepare_screen_window(error));
    CHECK(error.empty());
    CHECK(layout.future_screen_count() ==
          nmarkdown::kPlainTextFutureScreens);
    CHECK(source->read_calls == 0);

    CHECK(layout.seek_percentage(50, error));
    CHECK(error.empty());
    CHECK(!layout.visible_lines(error).empty());
    CHECK(source->read_calls == 0);

    const std::vector<nmarkdown::SearchMatch> matches = layout.search(
        "unique-streaming-search-needle",
        nmarkdown::SearchMode::ExactUtf8, 8, error);
    CHECK(error.empty());
    CHECK(matches.size() == 1);
    CHECK(source->read_calls == 0);
}

void test_full_cache_refills_forward_after_reaching_capacity() {
    const std::string content = sliding_cache_text();
    auto source = std::make_shared<CountingRandomAccess>(content);
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));

    nmarkdown::PlainTextLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(
        source, 0, static_cast<std::uint32_t>(content.size()), {}, text,
        signature, 220, error));
    CHECK(error.empty());
    CHECK(layout.raw_cache_size() ==
          nmarkdown::kPlainTextInitialCacheBytes);
    CHECK(source->bytes_read ==
          nmarkdown::kPlainTextInitialCacheBytes);

    // Grow the non-contiguous source cache all the way to its configured
    // capacity. The old regression jumped to 1 MiB while only 64 KiB was
    // resident, so it exercised ordinary cache growth rather than the
    // full-window forward-refill path.
    const std::size_t cache_capacity = layout.raw_cache_capacity();
    int fill_guard = 0;
    const int fill_guard_limit = static_cast<int>(
        cache_capacity / nmarkdown::kPlainTextPrefetchBytes + 512U);
    while (layout.raw_cache_size() < cache_capacity &&
           fill_guard < fill_guard_limit) {
        CHECK(layout.perform_incremental_work(error));
        CHECK(error.empty());
        ++fill_guard;
    }
    CHECK(fill_guard < fill_guard_limit);
    CHECK(layout.raw_cache_size() == cache_capacity);
    CHECK(source->bytes_read == cache_capacity);

    // Move the active page beyond the retained-history low watermark while
    // staying inside the resident cache. With a prepared screen window, the
    // next idle quantum must refill a new tail block without growing beyond
    // the fixed cache capacity.
    constexpr std::uint32_t kForwardPosition =
        20U * nmarkdown::kPlainTextPrefetchBytes;
    CHECK(layout.seek_source(kForwardPosition, error));
    CHECK(error.empty());
    CHECK(layout.current_source_offset() >=
          19U * nmarkdown::kPlainTextPrefetchBytes);
    layout.prepare_screen_window(error);
    CHECK(error.empty());

    const std::size_t before_refill = source->bytes_read;
    const std::size_t before_calls = source->read_calls;
    CHECK(layout.perform_incremental_work(error));
    CHECK(error.empty());
    CHECK(source->bytes_read ==
          before_refill + nmarkdown::kPlainTextPrefetchBytes);
    CHECK(source->read_calls == before_calls + 1);
    CHECK(layout.raw_cache_size() == cache_capacity);
    CHECK(!source->reads.empty());
    if (!source->reads.empty()) {
        const CountingRandomAccess::Read& refill = source->reads.back();
        CHECK(refill.offset == cache_capacity);
        CHECK(refill.size ==
              nmarkdown::kPlainTextPrefetchBytes);
    }

    // Rotate far enough that the logical cache crosses the physical end of
    // its allocation. Seeking and shaping near that seam must use the small
    // linearization scratch buffer, not reread or copy the full window.
    for (std::uint32_t rotation = 1; rotation < 12; ++rotation) {
        const std::uint32_t target =
            (rotation + 20U) *
            static_cast<std::uint32_t>(
                nmarkdown::kPlainTextPrefetchBytes);
        CHECK(layout.seek_source(target, error));
        CHECK(error.empty());
        layout.prepare_screen_window(error);
        CHECK(error.empty());
        const std::size_t before_rotation = source->bytes_read;
        CHECK(layout.perform_incremental_work(error));
        CHECK(error.empty());
        CHECK(source->bytes_read ==
              before_rotation + nmarkdown::kPlainTextPrefetchBytes);
        CHECK(layout.raw_cache_size() == cache_capacity);
    }

    constexpr std::uint32_t kRotations = 12;
    const std::uint32_t cache_start =
        kRotations *
        static_cast<std::uint32_t>(
            nmarkdown::kPlainTextPrefetchBytes);
    const std::uint32_t seam_target =
        cache_start +
        static_cast<std::uint32_t>(
            cache_capacity -
            cache_start -
            nmarkdown::kPlainTextPrefetchBytes / 2U);
    const std::size_t reads_before_seam = source->read_calls;
    CHECK(layout.seek_source(seam_target, error));
    CHECK(error.empty());
    CHECK(!layout.visible_lines(error).empty());
    CHECK(error.empty());
    CHECK(source->read_calls == reads_before_seam);
}

void test_thirty_forward_page_steps_are_monotonic() {
    const std::string content = large_text();
    auto source = std::make_shared<CountingRandomAccess>(content);
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));

    nmarkdown::PlainTextLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(
        source, 0, static_cast<std::uint32_t>(content.size()), {}, text,
        signature, 220, error));
    CHECK(error.empty());

    std::uint32_t previous = layout.current_source_offset();
    constexpr int kForwardPages = 30;
    for (int page = 0; page < kForwardPages; ++page) {
        int ready_guard = 0;
        while (layout.future_screen_count() <
                   nmarkdown::kPlainTextMinimumReadyScreens &&
               ready_guard < 128) {
            CHECK(layout.perform_incremental_work(error));
            CHECK(error.empty());
            ++ready_guard;
        }
        CHECK(ready_guard < 128);
        warm_ready_glyphs(layout);
        const std::size_t reads_before = source->read_calls;
        const std::uint64_t layout_steps_before =
            layout.incremental_layout_step_count();
        CHECK(layout.move_page(1, error));
        CHECK(error.empty());
        CHECK(source->read_calls == reads_before);
        CHECK(layout.incremental_layout_step_count() ==
              layout_steps_before);
        const std::uint32_t current = layout.current_source_offset();
        CHECK(current > previous);
        CHECK(layout.visible_source_end() > current);
        CHECK(layout.approximate_current_page() == page + 2);
        previous = current;
    }
    CHECK(previous < content.size());
    CHECK(layout.previous_screen_count() <=
          nmarkdown::kPlainTextPreviousScreens);
    CHECK(layout.future_screen_count() <=
          nmarkdown::kPlainTextFutureScreens);
}

void test_forward_page_defers_when_reserve_is_empty() {
    const std::string content = large_text();
    auto source = std::make_shared<CountingRandomAccess>(content);
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));

    nmarkdown::PlainTextLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(
        source, 0, static_cast<std::uint32_t>(content.size()), {}, text,
        signature, 220, error));
    CHECK(error.empty());
    CHECK(layout.future_screen_count() == 0);

    std::uint32_t start = layout.current_source_offset();
    int immediate_moves = 0;
    bool moved = false;
    do {
        const std::size_t event_reads = source->read_calls;
        const std::uint64_t event_steps =
            layout.incremental_layout_step_count();
        moved = layout.move_page(1, error);
        CHECK(source->read_calls == event_reads);
        CHECK(layout.incremental_layout_step_count() == event_steps);
        CHECK(error.empty());
        if (moved) {
            start = layout.current_source_offset();
            ++immediate_moves;
        }
    } while (moved && immediate_moves < 2);
    CHECK(immediate_moves < 2);
    CHECK(error.empty());
    const std::size_t reads_before = source->read_calls;
    const std::uint64_t steps_before =
        layout.incremental_layout_step_count();
    CHECK(layout.current_source_offset() == start);
    CHECK(layout.deferred_forward_page_count() == 1);
    CHECK(source->read_calls == reads_before);
    CHECK(layout.incremental_layout_step_count() == steps_before);
    CHECK(layout.pending_screen_line_count() == 0);

    int guard = 0;
    while (layout.current_source_offset() == start && guard < 256) {
        const std::size_t pending_before =
            layout.pending_screen_line_count();
        const std::size_t screens_before =
            layout.cached_screen_count();
        const std::uint64_t quantum_before =
            layout.incremental_layout_step_count();
        const std::size_t warmed =
            layout.preload_deferred_page_glyphs(4);
        const bool worked =
            warmed != 0 || layout.perform_incremental_work(error);
        CHECK(worked);
        CHECK(error.empty());
        const std::uint64_t quantum_after =
            layout.incremental_layout_step_count();
        CHECK(quantum_after == quantum_before ||
              quantum_after == quantum_before + 1);
        if (quantum_after == quantum_before + 1) {
            if (layout.cached_screen_count() == screens_before) {
                CHECK(layout.pending_screen_line_count() ==
                      pending_before + 1);
            } else {
                CHECK(layout.cached_screen_count() ==
                      screens_before + 1);
                CHECK(layout.pending_screen_line_count() == 0);
            }
        }
        ++guard;
    }
    CHECK(guard < 256);
    CHECK(layout.current_source_offset() > start);
    CHECK(layout.deferred_forward_page_count() == 0);
    const std::vector<nmarkdown::PlainTextVisibleLine> visible =
        layout.visible_lines(error);
    CHECK(error.empty());
    int visible_height = 0;
    for (const nmarkdown::PlainTextVisibleLine& item : visible) {
        if (item.line != nullptr) {
            visible_height +=
                nmarkdown::fx_ceil(item.line->advance);
        }
    }
    CHECK(visible_height >= 220 || layout.at_end());
}

void test_pixel_scroll_accumulates_without_cancelling() {
    const std::string content = large_text();
    auto source = std::make_shared<CountingRandomAccess>(content);
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));

    nmarkdown::PlainTextLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(
        source, 0, static_cast<std::uint32_t>(content.size()), {}, text,
        signature, 220, error));
    const std::uint32_t start = layout.current_source_offset();

    // Automatic 15 px body leading uses an 18 px drag threshold. Two partial
    // deltas should combine into exactly one forward line.
    CHECK(!layout.scroll_pixels(9, error));
    CHECK(layout.current_source_offset() == start);
    CHECK(layout.scroll_pixels(9, error));
    const std::uint32_t forward = layout.current_source_offset();
    CHECK(forward > start);

    CHECK(layout.scroll_pixels(-18, error));
    CHECK(layout.current_source_offset() == start);

    // A larger delta must remain monotonic instead of advancing and then
    // cancelling itself because move_line() reset the shared accumulator.
    CHECK(layout.scroll_pixels(54, error));
    CHECK(layout.current_source_offset() > forward);
}

void test_screen_window_page_offsets_seek_and_search() {
    const std::string content = large_text();
    auto source = std::make_shared<CountingRandomAccess>(content);
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));

    nmarkdown::PlainTextLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(
        source, 0, static_cast<std::uint32_t>(content.size()), {}, text,
        signature, 220, error));
    std::uint32_t previous = layout.current_source_offset();
    for (int page = 0; page < 9; ++page) {
        layout.prepare_screen_window(error);
        CHECK(error.empty());
        warm_ready_glyphs(layout);
        CHECK(layout.move_page(1, error));
        CHECK(error.empty());
        CHECK(layout.current_source_offset() > previous);
        previous = layout.current_source_offset();
    }
    CHECK(layout.previous_screen_count() <=
          nmarkdown::kPlainTextPreviousScreens);
    CHECK(layout.future_screen_count() <=
          nmarkdown::kPlainTextFutureScreens);
    const std::uint32_t forward = layout.current_source_offset();
    CHECK(layout.move_page(-1, error));
    CHECK(layout.current_source_offset() < forward);

    CHECK(layout.seek_percentage(50, error));
    CHECK(error.empty());
    const std::uint32_t halfway = layout.current_source_offset();
    CHECK(halfway > content.size() * 45U / 100U);
    CHECK(halfway < content.size() * 55U / 100U);
    CHECK(halfway == 0 || content[halfway - 1] == '\n' ||
          content[halfway - 1] == '\r');

    CHECK(layout.seek_percentage(100, error));
    CHECK(error.empty());
    CHECK(layout.current_source_offset() < content.size());
    CHECK(layout.at_end());
    CHECK(!layout.visible_lines(error).empty());
    CHECK(error.empty());

    const std::size_t reads_at_search_open = source->read_calls;
    const std::vector<nmarkdown::SearchMatch> matches = layout.search(
        "unique-streaming-search-needle",
        nmarkdown::SearchMode::ExactUtf8, 8, error);
    CHECK(error.empty());
    CHECK(matches.size() == 1);
    // Search remains a streaming scan opened only on demand. It may read
    // beyond the active 1 MiB window instead of forcing the whole novel into
    // RAM during launch.
    CHECK(source->read_calls > reads_at_search_open);
    CHECK(matches[0].source_offset > content.size() - 128U);
}

void test_page_step_skips_a_fully_visible_line() {
    std::string content;
    for (int line = 0; line < 32; ++line) {
        content += "page boundary line " + std::to_string(line) + "\n";
    }
    auto source = std::make_shared<CountingRandomAccess>(content);
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));

    nmarkdown::LayoutSignature signature;
    nmarkdown::PlainTextLayout probe;
    CHECK(probe.initialize(
        source, 0, static_cast<std::uint32_t>(content.size()), {}, text,
        signature, 220, error));
    const std::vector<nmarkdown::PlainTextVisibleLine> probe_lines =
        probe.visible_lines(error);
    CHECK(error.empty());
    CHECK(!probe_lines.empty());
    if (probe_lines.empty() || probe_lines[0].line == nullptr) return;
    const int one_line_viewport =
        std::max(1, nmarkdown::fx_ceil(probe_lines[0].line->advance));
    const std::uint32_t first_line_end =
        probe_lines[0].line->source_offset +
        probe_lines[0].line->source_length;

    nmarkdown::PlainTextLayout layout;
    CHECK(layout.initialize(
        source, 0, static_cast<std::uint32_t>(content.size()), {}, text,
        signature, one_line_viewport, error));
    CHECK(layout.current_source_offset() == 0);
    layout.prepare_screen_window(error);
    CHECK(error.empty());
    warm_ready_glyphs(layout);
    CHECK(layout.move_page(1, error));
    CHECK(error.empty());
    CHECK(layout.current_source_offset() >= first_line_end);
    CHECK(layout.move_page(-1, error));
    CHECK(layout.current_source_offset() == 0);
}

void test_page_up_after_line_scroll_uses_a_full_previous_viewport() {
    std::string content;
    for (int line = 0; line < 256; ++line) {
        content += "page-up cache regression line " +
                   std::to_string(line) + "\n";
    }
    auto source = std::make_shared<CountingRandomAccess>(content);
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));

    nmarkdown::LayoutSignature signature;
    nmarkdown::PlainTextLayout layout;
    CHECK(layout.initialize(
        source, 0, static_cast<std::uint32_t>(content.size()), {}, text,
        signature, 220, error));
    layout.prepare_screen_window(error);
    CHECK(error.empty());
    for (int line = 0; line < 5; ++line) {
        CHECK(layout.move_line(1, error));
        CHECK(error.empty());
    }

    const std::uint32_t before = layout.current_source_offset();
    CHECK(before > 0);
    CHECK(layout.move_page(-1, error));
    CHECK(error.empty());
    CHECK(layout.current_source_offset() < before);
    const std::vector<nmarkdown::PlainTextVisibleLine> visible =
        layout.visible_lines(error);
    CHECK(error.empty());
    CHECK(visible.size() > 1);
}

void test_long_paragraph_screen_build_is_split_across_idle_quanta() {
    std::string content;
    for (int index = 0; index < 900; ++index) {
        content += u8"红楼梦长段落排版测试，";
    }
    content += "\n";
    auto source = std::make_shared<CountingRandomAccess>(content);
    nmarkdown::TextSystem text;
    std::string error;
    CHECK(text.initialize(error));

    nmarkdown::PlainTextLayout layout;
    nmarkdown::LayoutSignature signature;
    CHECK(layout.initialize(
        source, 0, static_cast<std::uint32_t>(content.size()), {}, text,
        signature, 220, error));
    CHECK(error.empty());
    // The initial visible page is synchronous, but every speculative page is
    // assembled one bounded line probe at a time. A single idle call must not
    // shape a whole screen.
    CHECK(layout.cached_screen_count() == 1);
    CHECK(layout.future_screen_count() == 0);
    CHECK(layout.perform_incremental_work(error));
    CHECK(error.empty());
    CHECK(layout.cached_screen_count() == 1);
    int work_quanta = 1;
    while (layout.future_screen_count() == 0 && work_quanta < 64) {
        CHECK(layout.perform_incremental_work(error));
        CHECK(error.empty());
        ++work_quanta;
    }
    CHECK(work_quanta > 1);
    CHECK(work_quanta < 64);

    layout.prepare_screen_window(error);
    CHECK(error.empty());
    warm_ready_glyphs(layout);
    const std::size_t reads = source->read_calls;
    for (int page = 0; page < 4; ++page) {
        CHECK(layout.move_page(1, error));
        CHECK(error.empty());
    }
    CHECK(source->read_calls == reads);
}

}  // namespace

int main() {
    test_first_screen_cache_and_idle_prefetch();
    test_contiguous_source_is_not_copied_or_reread();
    test_full_cache_refills_forward_after_reaching_capacity();
    test_thirty_forward_page_steps_are_monotonic();
    test_forward_page_defers_when_reserve_is_empty();
    test_pixel_scroll_accumulates_without_cancelling();
    test_screen_window_page_offsets_seek_and_search();
    test_page_step_skips_a_fully_visible_line();
    test_page_up_after_line_scroll_uses_a_full_previous_viewport();
    test_long_paragraph_screen_build_is_split_across_idle_quanta();
    if (failures != 0) {
        std::fprintf(stderr, "%d plain-text layout test(s) failed\n",
                     failures);
        return 1;
    }
    std::printf("All plain-text layout tests passed\n");
    return 0;
}
