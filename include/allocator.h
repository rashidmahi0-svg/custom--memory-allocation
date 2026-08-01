#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <cstddef>

// Custom malloc/free implementation using sbrk() and a free-list.
// Supports First-Fit and Best-Fit placement strategies, block splitting,
// and coalescing of adjacent free blocks to fight fragmentation.
namespace calloc_lib {

enum class Strategy {
    FIRST_FIT,
    BEST_FIT
};

// Choose the placement strategy used by my_malloc().
void set_strategy(Strategy strategy);
Strategy get_strategy();

// Core allocator API (drop-in replacements for malloc/free/calloc/realloc).
void* my_malloc(size_t size);
void  my_free(void* ptr);
void* my_calloc(size_t num, size_t size);
void* my_realloc(void* ptr, size_t size);

// Introspection / debugging helpers.
void   print_heap_map();
size_t total_allocated_bytes();
size_t total_free_bytes();
size_t heap_extend_count(); // number of times sbrk() was called to grow the heap

} // namespace calloc_lib

#endif // ALLOCATOR_H
