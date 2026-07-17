#include "nmarkdown/platform/allocation_stats.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

// This file is linked only by `make ndless-memory-profile`. GNU ld redirects
// allocator calls to these wrappers while __real_* continues to call the
// Ndless/newlib allocator. A fixed table is used instead of allocation
// headers: it preserves the ABI alignment of returned pointers and safely
// tolerates memory returned by an unwrapped libc-internal allocation path.
namespace {

constexpr std::size_t kSlotCount = 8192;
constexpr std::uintptr_t kTombstone = 1;

struct AllocationSlot {
    std::uintptr_t address;
    std::size_t size;
};

AllocationSlot slots[kSlotCount];
std::uint64_t current_bytes;
std::uint64_t lifetime_peak_bytes;
std::uint64_t checkpoint_peak_bytes;
std::uint64_t allocation_count;
std::uint64_t allocation_failures;
std::uint64_t tracking_overflows;
bool checkpoint_started;

std::size_t slot_index(std::uintptr_t address) {
    // Allocator pointers are aligned, so discard their zero low bits before
    // applying a multiplicative hash. kSlotCount is a power of two.
    return static_cast<std::size_t>(
        ((address >> 3U) * static_cast<std::uintptr_t>(2654435761U)) &
        (kSlotCount - 1U));
}

bool find_slot(std::uintptr_t address, std::size_t& index) {
    std::size_t current = slot_index(address);
    for (std::size_t probe = 0; probe < kSlotCount; ++probe) {
        const std::uintptr_t candidate = slots[current].address;
        if (candidate == 0) return false;
        if (candidate == address) {
            index = current;
            return true;
        }
        current = (current + 1U) & (kSlotCount - 1U);
    }
    return false;
}

void update_peaks() {
    if (current_bytes > lifetime_peak_bytes) {
        lifetime_peak_bytes = current_bytes;
    }
    if (checkpoint_started && current_bytes > checkpoint_peak_bytes) {
        checkpoint_peak_bytes = current_bytes;
    }
}

bool insert_allocation(void* pointer, std::size_t size) {
    if (pointer == nullptr) return false;
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(pointer);
    std::size_t current = slot_index(address);
    std::size_t first_tombstone = kSlotCount;
    for (std::size_t probe = 0; probe < kSlotCount; ++probe) {
        const std::uintptr_t candidate = slots[current].address;
        if (candidate == address) {
            // This should occur only if a target allocator returns a live
            // address twice. Keep the accounting internally consistent.
            current_bytes -= slots[current].size;
            slots[current].size = size;
            current_bytes += size;
            update_peaks();
            return true;
        }
        if (candidate == kTombstone && first_tombstone == kSlotCount) {
            first_tombstone = current;
        } else if (candidate == 0) {
            const std::size_t destination =
                first_tombstone == kSlotCount ? current : first_tombstone;
            slots[destination] = {address, size};
            current_bytes += size;
            ++allocation_count;
            update_peaks();
            return true;
        }
        current = (current + 1U) & (kSlotCount - 1U);
    }
    if (first_tombstone != kSlotCount) {
        slots[first_tombstone] = {address, size};
        current_bytes += size;
        ++allocation_count;
        update_peaks();
        return true;
    }
    ++tracking_overflows;
    return false;
}

bool remove_allocation(void* pointer, std::size_t& size) {
    if (pointer == nullptr) return false;
    std::size_t index = 0;
    if (!find_slot(reinterpret_cast<std::uintptr_t>(pointer), index)) {
        return false;
    }
    size = slots[index].size;
    current_bytes -= size;
    slots[index].address = kTombstone;
    slots[index].size = 0;
    return true;
}

}  // namespace

extern "C" {

void* __real_malloc(std::size_t size);
void __real_free(void* pointer);
void* __real_realloc(void* pointer, std::size_t size);
void* __real_calloc(std::size_t count, std::size_t size);

void* __wrap_malloc(std::size_t size) {
    void* pointer = __real_malloc(size);
    if (pointer == nullptr && size != 0) {
        ++allocation_failures;
        std::printf(
            "NMARKDOWN_MEMORY/1 malloc_failure requested=%u current=%llu "
            "lifetime_peak=%llu allocations=%llu\n",
            static_cast<unsigned>(size),
            static_cast<unsigned long long>(current_bytes),
            static_cast<unsigned long long>(lifetime_peak_bytes),
            static_cast<unsigned long long>(allocation_count));
        std::fflush(stdout);
        return nullptr;
    }
    insert_allocation(pointer, size);
    return pointer;
}

void __wrap_free(void* pointer) {
    std::size_t ignored = 0;
    remove_allocation(pointer, ignored);
    __real_free(pointer);
}

void* __wrap_realloc(void* pointer, std::size_t size) {
    if (pointer == nullptr) return __wrap_malloc(size);
    if (size == 0) {
        __wrap_free(pointer);
        return nullptr;
    }

    std::size_t old_index = 0;
    const bool tracked = find_slot(
        reinterpret_cast<std::uintptr_t>(pointer), old_index);

    void* replacement = __real_realloc(pointer, size);
    if (replacement == nullptr) {
        ++allocation_failures;
        std::printf(
            "NMARKDOWN_MEMORY/1 realloc_failure requested=%u current=%llu "
            "lifetime_peak=%llu allocations=%llu\n",
            static_cast<unsigned>(size),
            static_cast<unsigned long long>(current_bytes),
            static_cast<unsigned long long>(lifetime_peak_bytes),
            static_cast<unsigned long long>(allocation_count));
        std::fflush(stdout);
        return nullptr;
    }

    if (tracked) {
        std::size_t removed_size = 0;
        remove_allocation(pointer, removed_size);
        insert_allocation(replacement, size);
    }
    return replacement;
}

void* __wrap_calloc(std::size_t count, std::size_t size) {
    if (size != 0 && count > static_cast<std::size_t>(-1) / size) {
        ++allocation_failures;
        return nullptr;
    }
    const std::size_t total = count * size;
    void* pointer = __real_calloc(count, size);
    if (pointer == nullptr && total != 0) {
        ++allocation_failures;
        std::printf(
            "NMARKDOWN_MEMORY/1 calloc_failure requested=%u current=%llu "
            "lifetime_peak=%llu allocations=%llu\n",
            static_cast<unsigned>(total),
            static_cast<unsigned long long>(current_bytes),
            static_cast<unsigned long long>(lifetime_peak_bytes),
            static_cast<unsigned long long>(allocation_count));
        std::fflush(stdout);
        return nullptr;
    }
    insert_allocation(pointer, total);
    return pointer;
}

}  // extern "C"

namespace nmarkdown {

AllocationStats allocation_stats() {
    AllocationStats result;
    result.available = true;
    result.checkpoint_available = checkpoint_started;
    result.current_bytes = current_bytes;
    result.lifetime_peak_bytes = lifetime_peak_bytes;
    result.checkpoint_peak_bytes = checkpoint_peak_bytes;
    result.allocation_count = allocation_count;
    result.allocation_failures = allocation_failures;
    result.tracking_overflows = tracking_overflows;
    return result;
}

void allocation_stats_begin_checkpoint() {
    checkpoint_started = true;
    checkpoint_peak_bytes = current_bytes;
}

}  // namespace nmarkdown
