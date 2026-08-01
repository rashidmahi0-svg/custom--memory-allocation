#ifndef LEAK_DETECTOR_H
#define LEAK_DETECTOR_H

#include <cstddef>

// Wraps calloc_lib::my_malloc/my_free to track every live allocation's
// size and call-site (file:line), so unfreed blocks can be reported.
namespace leak_lib {

void* tracked_malloc(size_t size, const char* file, int line);
void  tracked_free(void* ptr);

// Print every allocation that hasn't been freed yet.
void   report_leaks();
size_t leak_count();

} // namespace leak_lib

// Convenience macros that automatically capture the call site.
#define LMALLOC(size) leak_lib::tracked_malloc((size), __FILE__, __LINE__)
#define LFREE(ptr)    leak_lib::tracked_free((ptr))

#endif // LEAK_DETECTOR_H
