#include "leak_detector.h"
#include "allocator.h"

#include <unordered_map>
#include <mutex>
#include <iostream>

namespace leak_lib {
namespace {

struct AllocInfo {
    size_t      size;
    const char* file;
    int         line;
};

std::unordered_map<void*, AllocInfo> live_allocations;
std::mutex leak_mutex;

} // namespace

void* tracked_malloc(size_t size, const char* file, int line) {
    void* ptr = calloc_lib::my_malloc(size);
    if (ptr) {
        std::lock_guard<std::mutex> lock(leak_mutex);
        live_allocations[ptr] = AllocInfo{size, file, line};
    }
    return ptr;
}

void tracked_free(void* ptr) {
    if (!ptr) return;
    {
        std::lock_guard<std::mutex> lock(leak_mutex);
        auto it = live_allocations.find(ptr);
        if (it == live_allocations.end()) {
            std::cerr << "[leak-detector] warning: freeing untracked/already-freed pointer "
                      << ptr << "\n";
        } else {
            live_allocations.erase(it);
        }
    }
    calloc_lib::my_free(ptr);
}

void report_leaks() {
    std::lock_guard<std::mutex> lock(leak_mutex);
    if (live_allocations.empty()) {
        std::cout << "[leak-detector] No memory leaks detected.\n";
        return;
    }
    std::cout << "[leak-detector] " << live_allocations.size()
              << " memory leak(s) detected:\n";
    size_t total = 0;
    for (const auto& [ptr, info] : live_allocations) {
        std::cout << "  - " << info.size << " bytes leaked at " << ptr
                  << " (allocated at " << info.file << ":" << info.line << ")\n";
        total += info.size;
    }
    std::cout << "  Total leaked: " << total << " bytes\n";
}

size_t leak_count() {
    std::lock_guard<std::mutex> lock(leak_mutex);
    return live_allocations.size();
}

} // namespace leak_lib
