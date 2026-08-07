#include "buddy_allocator.h"

#include <sys/mman.h>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <mutex>

namespace buddy_lib {
namespace {

constexpr int    MIN_ORDER          = 5;  // smallest block = 2^5 = 32 bytes
constexpr size_t HEADER_SIZE_ALIGNED = 16; // bytes reserved for BuddyHeader
constexpr uint32_t MAGIC             = 0xB0DDA110;

// Free blocks store a singly linked list node in their own (unused) memory.
struct FreeNode {
    FreeNode* next;
};

// Every allocated block starts with this header so buddy_free() knows
// how large the block was without the caller having to pass it back.
struct BuddyHeader {
    uint32_t order;
    uint32_t magic;
};

char*      pool_base  = nullptr;
size_t     pool_size  = 0;
int        max_order  = 0;
FreeNode** free_lists  = nullptr; // free_lists[order] = head of free list for that order
std::mutex buddy_mutex;

size_t next_pow2(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

int order_for_size(size_t size) {
    size_t total = size + HEADER_SIZE_ALIGNED;
    int order = MIN_ORDER;
    size_t block = static_cast<size_t>(1) << order;
    while (block < total) {
        order++;
        block <<= 1;
    }
    return order;
}

void list_push(int order, void* addr) {
    FreeNode* node = reinterpret_cast<FreeNode*>(addr);
    node->next = free_lists[order];
    free_lists[order] = node;
}

void* list_pop(int order) {
    if (!free_lists[order]) return nullptr;
    FreeNode* node = free_lists[order];
    free_lists[order] = node->next;
    return reinterpret_cast<void*>(node);
}

bool list_remove(int order, void* addr) {
    FreeNode** cur = &free_lists[order];
    while (*cur) {
        if (reinterpret_cast<void*>(*cur) == addr) {
            *cur = (*cur)->next;
            return true;
        }
        cur = &(*cur)->next;
    }
    return false;
}

} // namespace

bool buddy_init(size_t requested_size) {
    std::lock_guard<std::mutex> lock(buddy_mutex);

    pool_size = next_pow2(requested_size);
    max_order = static_cast<int>(std::log2(static_cast<double>(pool_size)));

    void* mem = mmap(nullptr, pool_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        std::cerr << "[buddy] mmap failed\n";
        return false;
    }
    pool_base = reinterpret_cast<char*>(mem);

    free_lists = new FreeNode*[max_order + 1];
    for (int i = 0; i <= max_order; i++) free_lists[i] = nullptr;

    list_push(max_order, pool_base); // whole pool starts as one free block
    return true;
}

void buddy_destroy() {
    std::lock_guard<std::mutex> lock(buddy_mutex);
    if (pool_base) munmap(pool_base, pool_size);
    delete[] free_lists;
    pool_base  = nullptr;
    free_lists = nullptr;
    pool_size  = 0;
    max_order  = 0;
}

void* buddy_alloc(size_t size) {
    std::lock_guard<std::mutex> lock(buddy_mutex);
    if (!pool_base || size == 0) return nullptr;

    int needed_order = order_for_size(size);
    if (needed_order > max_order) return nullptr; // request bigger than pool

    int order = needed_order;
    while (order <= max_order && !free_lists[order]) order++;
    if (order > max_order) return nullptr; // pool exhausted

    void* block = list_pop(order);

    // Split the block down to the needed order, stashing each leftover
    // "buddy" half onto its own free list.
    while (order > needed_order) {
        order--;
        size_t half = static_cast<size_t>(1) << order;
        char* buddy_addr = reinterpret_cast<char*>(block) + half;
        list_push(order, buddy_addr);
    }

    BuddyHeader* header = reinterpret_cast<BuddyHeader*>(block);
    header->order = static_cast<uint32_t>(needed_order);
    header->magic = MAGIC;

    return reinterpret_cast<char*>(block) + HEADER_SIZE_ALIGNED;
}

void buddy_free(void* ptr) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(buddy_mutex);

    char* block = reinterpret_cast<char*>(ptr) - HEADER_SIZE_ALIGNED;
    BuddyHeader* header = reinterpret_cast<BuddyHeader*>(block);
    if (header->magic != MAGIC) {
        std::cerr << "[buddy] invalid free / corruption detected\n";
        return;
    }
    int order = static_cast<int>(header->order);
    header->magic = 0; // invalidate so a double-free is caught next time

    size_t offset = static_cast<size_t>(block - pool_base);

    // Repeatedly try to merge with the buddy block. Buddy address is found
    // by flipping the bit at position `order` in the offset.
    while (order < max_order) {
        size_t buddy_offset = offset ^ (static_cast<size_t>(1) << order);
        char* buddy_addr = pool_base + buddy_offset;
        if (!list_remove(order, buddy_addr)) {
            break; // buddy is not free (or not the same size) - stop merging
        }
        offset = offset & buddy_offset; // lower address of the pair survives
        order++;
        block = pool_base + offset;
    }
    list_push(order, block);
}

void print_buddy_map() {
    std::lock_guard<std::mutex> lock(buddy_mutex);
    std::cout << "----- Buddy Free Lists -----\n";
    for (int i = MIN_ORDER; i <= max_order; i++) {
        int count = 0;
        for (FreeNode* n = free_lists[i]; n; n = n->next) count++;
        std::cout << "  order " << i << " (block size " << (static_cast<size_t>(1) << i)
                  << " bytes): " << count << " free block(s)\n";
    }
    std::cout << "-----------------------------\n";
}

} // namespace buddy_lib
