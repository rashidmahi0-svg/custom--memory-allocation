#ifndef BUDDY_ALLOCATOR_H
#define BUDDY_ALLOCATOR_H

#include <cstddef>

// Buddy-system allocator built on top of a single mmap()'d pool.
// The pool size is rounded up to a power of two. Allocation requests are
// rounded up to the nearest power-of-two block; freeing a block repeatedly
// tries to merge it with its "buddy" to reduce fragmentation.
namespace buddy_lib {

// Reserve a pool of at least `pool_size` bytes (rounded up to a power of two)
// via mmap(). Must be called before buddy_alloc()/buddy_free().
bool buddy_init(size_t pool_size);

// Release the pool back to the OS via munmap().
void buddy_destroy();

void* buddy_alloc(size_t size);
void  buddy_free(void* ptr);

void print_buddy_map();

} // namespace buddy_lib

#endif // BUDDY_ALLOCATOR_H
