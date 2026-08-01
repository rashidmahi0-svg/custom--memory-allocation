#include "allocator.h"

#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <iostream>

namespace calloc_lib {
namespace {

constexpr size_t ALIGNMENT   = 16; // all payloads aligned to 16 bytes
constexpr size_t MIN_PAYLOAD = 16; // don't split off a fragment smaller than this

// Block header sits immediately before the memory returned to the caller.
// Blocks form a doubly linked list IN ADDRESS ORDER. Because sbrk() grows
// the heap contiguously, `next`/`prev` in this list are also physically
// adjacent in memory -- which is exactly what coalescing needs.
struct BlockHeader {
    size_t       size;     // usable payload size (aligned), excludes header
    bool         is_free;
    BlockHeader* next;
    BlockHeader* prev;
};

constexpr size_t HEADER_SIZE = sizeof(BlockHeader);

BlockHeader* heap_head = nullptr;
BlockHeader* heap_tail = nullptr;
Strategy     strategy  = Strategy::FIRST_FIT;
std::mutex   alloc_mutex;

size_t stat_total_allocated = 0;
size_t stat_total_free      = 0;
size_t stat_extend_count    = 0;

size_t align_up(size_t n, size_t align) {
    return (n + align - 1) & ~(align - 1);
}

// Ask the OS for more heap space via sbrk() and append a new block.
BlockHeader* request_space(BlockHeader* last, size_t size) {
    void* requested = sbrk(0);
    void* result = sbrk(static_cast<intptr_t>(HEADER_SIZE + size));
    if (result == reinterpret_cast<void*>(-1)) {
        return nullptr; // sbrk failed - out of memory
    }
    BlockHeader* block = reinterpret_cast<BlockHeader*>(requested);
    block->size    = size;
    block->is_free = false;
    block->next    = nullptr;
    block->prev    = last;
    if (last) last->next = block;
    stat_extend_count++;
    return block;
}

BlockHeader* find_fit(size_t size) {
    BlockHeader* current = heap_head;
    BlockHeader* best    = nullptr;
    while (current) {
        if (current->is_free && current->size >= size) {
            if (strategy == Strategy::FIRST_FIT) {
                return current;
            } else { // BEST_FIT: keep the smallest block that still fits
                if (!best || current->size < best->size) best = current;
            }
        }
        current = current->next;
    }
    return best;
}

// Split `block` so the first `size` bytes are returned to the caller and
// the remainder becomes a new free block, provided the remainder is
// large enough to be useful on its own.
void split_block(BlockHeader* block, size_t size) {
    if (block->size >= size + HEADER_SIZE + MIN_PAYLOAD) {
        BlockHeader* new_block = reinterpret_cast<BlockHeader*>(
            reinterpret_cast<char*>(block) + HEADER_SIZE + size);
        new_block->size    = block->size - size - HEADER_SIZE;
        new_block->is_free = true;
        new_block->next    = block->next;
        new_block->prev    = block;
        if (new_block->next) new_block->next->prev = new_block;
        else heap_tail = new_block;

        block->size = size;
        block->next = new_block;
    }
}

// Merge `block` with its physical neighbours if they are also free.
BlockHeader* coalesce(BlockHeader* block) {
    if (block->next && block->next->is_free) {
        block->size += HEADER_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
        else heap_tail = block;
    }
    if (block->prev && block->prev->is_free) {
        block->prev->size += HEADER_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
        else heap_tail = block->prev;
        block = block->prev;
    }
    return block;
}

BlockHeader* ptr_to_header(void* ptr) {
    return reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(ptr) - HEADER_SIZE);
}

} // namespace

void set_strategy(Strategy s) {
    std::lock_guard<std::mutex> lock(alloc_mutex);
    strategy = s;
}

Strategy get_strategy() { return strategy; }

void* my_malloc(size_t size) {
    if (size == 0) return nullptr;
    std::lock_guard<std::mutex> lock(alloc_mutex);

    size = align_up(size, ALIGNMENT);

    BlockHeader* block;
    if (!heap_head) {
        block = request_space(nullptr, size);
        if (!block) return nullptr;
        heap_head = block;
        heap_tail = block;
    } else {
        block = find_fit(size);
        if (block) {
            split_block(block, size);
            block->is_free = false;
        } else {
            block = request_space(heap_tail, size);
            if (!block) return nullptr;
            heap_tail = block;
        }
    }

    stat_total_allocated += block->size;
    return reinterpret_cast<char*>(block) + HEADER_SIZE;
}

void my_free(void* ptr) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(alloc_mutex);

    BlockHeader* block = ptr_to_header(ptr);
    if (block->is_free) {
        std::cerr << "[allocator] warning: double free detected at " << ptr << "\n";
        return;
    }
    block->is_free = true;
    stat_total_allocated -= block->size;
    stat_total_free      += block->size;
    coalesce(block);
}

void* my_calloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = my_malloc(total);
    if (ptr) std::memset(ptr, 0, total);
    return ptr;
}

void* my_realloc(void* ptr, size_t size) {
    if (!ptr) return my_malloc(size);
    if (size == 0) { my_free(ptr); return nullptr; }

    BlockHeader* block;
    {
        std::lock_guard<std::mutex> lock(alloc_mutex);
        block = ptr_to_header(ptr);
        if (block->size >= align_up(size, ALIGNMENT)) return ptr; // fits already
    }

    void* new_ptr = my_malloc(size);
    if (!new_ptr) return nullptr;
    std::memcpy(new_ptr, ptr, block->size);
    my_free(ptr);
    return new_ptr;
}

void print_heap_map() {
    std::lock_guard<std::mutex> lock(alloc_mutex);
    BlockHeader* current = heap_head;
    std::cout << "----- Heap Map (" << (strategy == Strategy::FIRST_FIT ? "First-Fit" : "Best-Fit") << ") -----\n";
    int i = 0;
    while (current) {
        std::cout << "  [" << i++ << "] addr=" << static_cast<void*>(current)
                  << " size=" << current->size
                  << " status=" << (current->is_free ? "FREE" : "USED") << "\n";
        current = current->next;
    }
    std::cout << "-----------------------------------\n";
}

size_t total_allocated_bytes() { return stat_total_allocated; }
size_t total_free_bytes()      { return stat_total_free; }
size_t heap_extend_count()     { return stat_extend_count; }

} // namespace calloc_lib
