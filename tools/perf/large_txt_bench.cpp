#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <cstdlib>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include "nmarkdown/app/viewer.h"
#include "nmarkdown/document/utf8.h"
#include "nmarkdown/platform/platform.h"
#include "nmarkdown/render/surface565.h"

namespace {

using SteadyClock = std::chrono::steady_clock;

class BenchmarkClock final : public nmarkdown::Clock {
public:
    BenchmarkClock() : started_(SteadyClock::now()) {}

    std::uint64_t milliseconds() const override {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                SteadyClock::now() - started_).count());
    }

    void sleep_ms(std::uint32_t) override {}

private:
    SteadyClock::time_point started_;
};

class CountingRandomAccessData final : public nmarkdown::RandomAccessData {
public:
    explicit CountingRandomAccessData(
        std::shared_ptr<nmarkdown::RandomAccessData> source,
        std::uint32_t penalty_us = 0)
        : source_(std::move(source)),
          penalty_us_(penalty_us) {}

    std::uint64_t size() const override {
        return source_ == nullptr ? 0 : source_->size();
    }

    bool read(std::uint64_t offset,
              std::uint8_t* data,
              std::size_t size) override {
        if (source_ == nullptr) return false;
        const SteadyClock::time_point begin = SteadyClock::now();
        if (penalty_us_ != 0) {
            std::this_thread::sleep_for(
                std::chrono::microseconds(penalty_us_));
        }
        const bool result = source_->read(offset, data, size);
        read_ms += std::chrono::duration<double, std::milli>(
            SteadyClock::now() - begin).count();
        ++read_calls;
        bytes_read += size;
        return result;
    }

    std::uint64_t read_calls = 0;
    std::uint64_t bytes_read = 0;
    double read_ms = 0;

private:
    std::shared_ptr<nmarkdown::RandomAccessData> source_;
    std::uint32_t penalty_us_ = 0;
};

template <typename Function>
double timed_ms(Function&& function) {
    const SteadyClock::time_point begin = SteadyClock::now();
    function();
    return std::chrono::duration<double, std::milli>(
        SteadyClock::now() - begin).count();
}

bool continuation(std::uint8_t value) {
    return (value & 0xC0U) == 0x80U;
}

std::size_t complete_utf8_prefix(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty()) return 0;
    std::size_t lead = bytes.size() - 1;
    std::size_t continuation_count = 0;
    while (lead > 0 && continuation(bytes[lead]) &&
           continuation_count < 3) {
        --lead;
        ++continuation_count;
    }
    const std::uint8_t first = bytes[lead];
    std::size_t expected = 1;
    if (first >= 0xC2U && first <= 0xDFU) expected = 2;
    else if (first >= 0xE0U && first <= 0xEFU) expected = 3;
    else if (first >= 0xF0U && first <= 0xF4U) expected = 4;
    return bytes.size() - lead < expected ? lead : bytes.size();
}

bool install_cjk(
    nmarkdown::Viewer& viewer,
    const std::shared_ptr<CountingRandomAccessData>& source,
    std::uint32_t signature,
    bool resident,
    std::string& error) {
    nmarkdown::FontRegistryState registry;
    constexpr nmarkdown::FontFaceId kCjkFace = 9100;
    if (resident) {
        // Mirror the application's promotion path: one sequential read of
        // the whole payload through the counted (penalized) source, then a
        // RAM-resident registry entry with no stream.
        std::vector<std::uint8_t> payload(
            static_cast<std::size_t>(source->size()));
        if (!source->read(0, payload.data(), payload.size())) {
            error = "could not read the font payload into memory";
            return false;
        }
        registry.fonts.push_back(
            {kCjkFace,
             std::make_shared<const std::vector<std::uint8_t>>(
                 std::move(payload)),
             nullptr, signature});
    } else {
        registry.fonts.push_back(
            {kCjkFace, nullptr, source, signature});
    }
    registry.roles[static_cast<std::size_t>(
        nmarkdown::external_font_role_index(
            nmarkdown::FontRole::Cjk))] = kCjkFace;
    std::array<std::string, nmarkdown::kExternalFontRoleCount> labels{};
    labels[static_cast<std::size_t>(
        nmarkdown::external_font_role_index(
            nmarkdown::FontRole::Cjk))] =
        resident ? "Resident CJK" : "Streamed CJK";
    return viewer.set_font_registry(
        std::move(registry), labels, error);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: nmarkdown-large-txt-bench FILE.txt [CJK.ttf] "
                     "[idle-polls] [page-count>=30] "
                     "[font-read-penalty-us] [resident-font=0|1]\n");
        return 2;
    }
    const char* document_path = argv[1];
    const char* cjk_path = argc > 2
                               ? argv[2]
                               : "assets/fonts/SarasaFixedSC-Regular-CX.ttf";
    const int idle_polls = argc > 3 ? std::max(0, std::atoi(argv[3])) : 0;
    const int page_count =
        argc > 4 ? std::max(30, std::atoi(argv[4])) : 60;
    const std::uint32_t font_read_penalty_us =
        argc > 5
            ? static_cast<std::uint32_t>(
                  std::max(0, std::atoi(argv[5])))
            : 0;
    const bool resident_font = argc > 6 && std::atoi(argv[6]) != 0;

    std::string error;
    nmarkdown::StdioFileSystem files;
    nmarkdown::DocumentProbe probe;
    nmarkdown::DocumentProbe cjk_probe;
    std::shared_ptr<nmarkdown::RandomAccessData> source_data;
    std::shared_ptr<nmarkdown::RandomAccessData> cjk_data;
    bool opened = false;
    const double open_ms = timed_ms([&] {
        opened = files.probe(document_path, probe, error) &&
                 files.open_random_access(document_path, source_data, error) &&
                 files.probe(cjk_path, cjk_probe, error) &&
                 files.open_random_access(cjk_path, cjk_data, error);
    });
    if (!opened || source_data == nullptr ||
        cjk_data == nullptr ||
        source_data->size() != probe.size || probe.size == 0 ||
        cjk_data->size() != cjk_probe.size || cjk_probe.size == 0 ||
        probe.size > std::numeric_limits<std::uint32_t>::max()) {
        std::fprintf(stderr, "could not open input: %s\n", error.c_str());
        return 1;
    }
    auto counted_cjk =
        std::make_shared<CountingRandomAccessData>(
            std::move(cjk_data), font_read_penalty_us);

    constexpr std::size_t kValidationProbeBytes = 64U * 1024U + 4U;
    std::vector<std::uint8_t> validation_probe(
        static_cast<std::size_t>(std::min<std::uint64_t>(
            probe.size, kValidationProbeBytes)));
    if (!source_data->read(
            0, validation_probe.data(), validation_probe.size())) {
        std::fprintf(stderr, "could not read UTF-8 validation probe\n");
        return 1;
    }
    validation_probe.resize(complete_utf8_prefix(validation_probe));
    const nmarkdown::Utf8ValidationResult validation =
        nmarkdown::utf8_validate(validation_probe.data(),
                                 validation_probe.size(), false);
    if (!validation.valid()) {
        std::fprintf(stderr, "benchmark input probe is not UTF-8\n");
        return 1;
    }

    nmarkdown::Viewer viewer;
    bool font_loaded = false;
    const double font_ms = timed_ms([&] {
        font_loaded = install_cjk(
            viewer, counted_cjk, cjk_probe.sample_hash, resident_font,
            error);
    });
    if (!font_loaded) {
        std::fprintf(stderr, "font failed: %s\n", error.c_str());
        return 1;
    }
    bool set = false;
    const double set_ms = timed_ms([&] {
        set = viewer.set_plain_text_document(
            source_data, 0, static_cast<std::uint32_t>(probe.size),
            validation, probe, error);
        viewer.set_reading_mode(nmarkdown::ReadingMode::HorizontalScroll);
    });
    if (!set) {
        std::fprintf(stderr, "viewer failed: %s\n", error.c_str());
        return 1;
    }

    constexpr int kWidth = 320;
    constexpr int kHeight = 240;
    std::vector<std::uint16_t> pixels(kWidth * kHeight);
    nmarkdown::Surface565 surface(pixels.data(), kWidth, kHeight, kWidth);
    BenchmarkClock work_clock;

    const double first_render_ms = timed_ms([&] { viewer.render(surface); });
    const nmarkdown::GlyphCacheStats first_glyphs = viewer.glyph_cache_stats();
    const std::uint64_t first_render_font_calls =
        counted_cjk->read_calls;
    const std::uint64_t first_render_font_bytes =
        counted_cjk->bytes_read;

    std::vector<double> idle_ms;
    idle_ms.reserve(static_cast<std::size_t>(idle_polls) * 25U);
    for (int index = 0; index < idle_polls; ++index) {
        idle_ms.push_back(timed_ms([&] {
            viewer.perform_incremental_work(
                work_clock, work_clock.milliseconds() + 2);
        }));
    }

    std::vector<double> event_ms;
    std::vector<double> render_ms;
    std::vector<double> page_visible_ms;
    std::vector<std::uint64_t> glyph_misses;
    std::vector<std::uint32_t> page_offsets;
    int deferred_page_polls = 0;
    int maximum_deferred_page_polls = 0;
    std::uint64_t page_font_reads = 0;
    std::uint64_t maximum_page_font_reads = 0;
    std::uint64_t maximum_poll_font_reads = 0;
    std::uint64_t page_font_bytes = 0;
    double maximum_deferred_poll_ms = 0;
    page_offsets.reserve(static_cast<std::size_t>(page_count) + 1U);
    page_offsets.push_back(viewer.reader_state(0).position.source_offset);
    for (int page = 0; page < page_count; ++page) {
        const SteadyClock::time_point page_begin =
            SteadyClock::now();
        const std::uint64_t reads_before =
            counted_cjk->read_calls;
        const std::uint64_t bytes_before =
            counted_cjk->bytes_read;
        event_ms.push_back(timed_ms([&] {
            // Natural swiping is the default: swiping down turns to the next
            // page, matching the REAL_NOVEL Firebird fixture.
            viewer.handle_event({nmarkdown::InputEventType::SwipeDown, 0});
        }));
        std::uint32_t offset =
            viewer.reader_state(0).position.source_offset;
        int deferred_polls = 0;
        while (offset <= page_offsets.back() &&
               deferred_polls < 2048) {
            const std::uint64_t poll_reads_before =
                counted_cjk->read_calls;
            const double poll_ms = timed_ms([&] {
                viewer.perform_incremental_work(
                    work_clock, work_clock.milliseconds() + 2);
            });
            maximum_deferred_poll_ms =
                std::max(maximum_deferred_poll_ms, poll_ms);
            maximum_poll_font_reads = std::max(
                maximum_poll_font_reads,
                counted_cjk->read_calls - poll_reads_before);
            offset = viewer.reader_state(0).position.source_offset;
            ++deferred_polls;
        }
        deferred_page_polls += deferred_polls;
        maximum_deferred_page_polls =
            std::max(maximum_deferred_page_polls, deferred_polls);
        if (offset <= page_offsets.back()) {
            std::fprintf(stderr,
                         "page %d did not advance: %u -> %u\n",
                         page + 1, page_offsets.back(), offset);
            return 1;
        }
        page_offsets.push_back(offset);
        const std::uint64_t before = viewer.glyph_cache_stats().misses;
        render_ms.push_back(timed_ms([&] { viewer.render(surface); }));
        page_visible_ms.push_back(
            std::chrono::duration<double, std::milli>(
                SteadyClock::now() - page_begin).count());
        glyph_misses.push_back(viewer.glyph_cache_stats().misses - before);
        const std::uint64_t page_reads =
            counted_cjk->read_calls - reads_before;
        page_font_reads += page_reads;
        maximum_page_font_reads =
            std::max(maximum_page_font_reads, page_reads);
        page_font_bytes += counted_cjk->bytes_read - bytes_before;
        for (int index = 0; index < idle_polls; ++index) {
            idle_ms.push_back(timed_ms([&] {
                viewer.perform_incremental_work(
                    work_clock, work_clock.milliseconds() + 2);
            }));
        }
    }

    const auto average = [](const std::vector<double>& values) {
        double sum = 0;
        for (double value : values) sum += value;
        return values.empty() ? 0.0 : sum / values.size();
    };
    const auto maximum = [](const std::vector<double>& values) {
        return values.empty() ? 0.0
                              : *std::max_element(values.begin(), values.end());
    };
    std::vector<double> sorted = render_ms;
    std::sort(sorted.begin(), sorted.end());
    const double p95 = sorted.empty() ? 0.0 : sorted[(sorted.size() * 95U) / 100U];
    std::vector<double> sorted_visible = page_visible_ms;
    std::sort(sorted_visible.begin(), sorted_visible.end());
    const double visible_p95 =
        sorted_visible.empty()
            ? 0.0
            : sorted_visible[
                  (sorted_visible.size() * 95U) / 100U];
    std::uint64_t page_misses = 0;
    for (std::uint64_t misses : glyph_misses) page_misses += misses;

    nmarkdown::Viewer line_viewer;
    if (!install_cjk(
            line_viewer, counted_cjk, cjk_probe.sample_hash, resident_font,
            error) ||
        !line_viewer.set_plain_text_document(
            source_data, 0, static_cast<std::uint32_t>(probe.size),
            validation, probe, error)) {
        std::fprintf(stderr, "line-scroll viewer failed: %s\n",
                     error.c_str());
        return 1;
    }
    line_viewer.set_reading_mode(nmarkdown::ReadingMode::VerticalScroll);
    line_viewer.render(surface);
    std::vector<double> line_event_ms;
    std::vector<double> line_render_ms;
    std::uint64_t line_misses = 0;
    for (int line = 0; line < 120; ++line) {
        line_event_ms.push_back(timed_ms([&] {
            line_viewer.handle_event(
                {nmarkdown::InputEventType::ScrollLineDown, 0});
        }));
        const std::uint64_t before =
            line_viewer.glyph_cache_stats().misses;
        line_render_ms.push_back(
            timed_ms([&] { line_viewer.render(surface); }));
        line_misses += line_viewer.glyph_cache_stats().misses - before;
    }
    std::vector<double> sorted_lines = line_render_ms;
    std::sort(sorted_lines.begin(), sorted_lines.end());
    const double line_p95 =
        sorted_lines.empty()
            ? 0.0
            : sorted_lines[(sorted_lines.size() * 95U) / 100U];

    std::printf("bytes=%llu txt_ir=0 global_line_index=0 "
                "file_backed=1 open=%.3fms font=%.3fms set=%.3fms\n",
                static_cast<unsigned long long>(probe.size), open_ms, font_ms,
                set_ms);
    std::printf("first_render=%.3fms post_present=removed "
                "glyph_miss=%llu glyph_hit=%llu "
                "font_reads=%llu font_bytes=%llu\n",
                first_render_ms,
                static_cast<unsigned long long>(first_glyphs.misses),
                static_cast<unsigned long long>(first_glyphs.hits),
                static_cast<unsigned long long>(first_render_font_calls),
                static_cast<unsigned long long>(first_render_font_bytes));
    std::printf("idle_polls=%d idle_avg=%.3fms idle_max=%.3fms\n",
                idle_polls, average(idle_ms), maximum(idle_ms));
    std::printf("swipe%d event_avg=%.3fms event_max=%.3fms "
                "render_avg=%.3fms render_p95=%.3fms render_max=%.3fms "
                "glyph_miss=%llu cache_entries=%zu cache_pages=%zu "
                "cache_evictions=%llu scroll=%d deferred_polls=%d "
                "deferred_max=%d font_reads=%llu font_reads_max=%llu "
                "font_bytes=%llu visible_avg=%.3fms "
                "visible_p95=%.3fms visible_max=%.3fms "
                "poll_read_max=%llu poll_max=%.3fms\n",
                page_count, average(event_ms), maximum(event_ms),
                average(render_ms), p95, maximum(render_ms),
                static_cast<unsigned long long>(page_misses),
                viewer.glyph_cache_stats().entries,
                viewer.glyph_cache_stats().pages,
                static_cast<unsigned long long>(
                    viewer.glyph_cache_stats().evictions),
                viewer.scroll_y(), deferred_page_polls,
                maximum_deferred_page_polls,
                static_cast<unsigned long long>(page_font_reads),
                static_cast<unsigned long long>(maximum_page_font_reads),
                static_cast<unsigned long long>(page_font_bytes),
                average(page_visible_ms), visible_p95,
                maximum(page_visible_ms),
                static_cast<unsigned long long>(
                    maximum_poll_font_reads),
                maximum_deferred_poll_ms);
    std::printf("swipe%d source_start=%u source_end=%u\n",
                page_count,
                page_offsets.front(), page_offsets.back());
    std::printf("line120 event_avg=%.3fms event_max=%.3fms "
                "render_avg=%.3fms render_p95=%.3fms render_max=%.3fms "
                "glyph_miss=%llu\n",
                average(line_event_ms), maximum(line_event_ms),
                average(line_render_ms), line_p95, maximum(line_render_ms),
                static_cast<unsigned long long>(line_misses));
    std::printf("font_io_total reads=%llu bytes=%llu host_read=%.3fms "
                "penalty_us=%u\n",
                static_cast<unsigned long long>(counted_cjk->read_calls),
                static_cast<unsigned long long>(counted_cjk->bytes_read),
                counted_cjk->read_ms, font_read_penalty_us);
    return 0;
}
