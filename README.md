# Custom Memory Allocator (C++)

A from-scratch reimplementation of `malloc()`/`free()` for Linux, built in three layers:

1. **Core allocator** (`allocator.h/.cpp`) — a free-list allocator that grows the
   heap with `sbrk()`, supports **First-Fit** and **Best-Fit** placement,
   splits blocks, and **coalesces** adjacent free blocks to fight fragmentation.
2. **Buddy system allocator** (`buddy_allocator.h/.cpp`) — a power-of-two
   buddy allocator built on a single `mmap()` pool, with block splitting and
   buddy-merging on free.
3. **Leak detector** (`leak_detector.h/.cpp`) — wraps the core allocator to
   track every live allocation's size and call site (`file:line`), and
   reports anything never freed.

## Project layout

```
custom_allocator/
├── include/
│   ├── allocator.h         # First-Fit / Best-Fit free-list allocator API
│   ├── buddy_allocator.h   # Buddy system allocator API
│   └── leak_detector.h     # Leak tracking API (LMALLOC / LFREE macros)
├── src/
│   ├── allocator.cpp
│   ├── buddy_allocator.cpp
│   ├── leak_detector.cpp
│   └── main.cpp            # Demo suite / interactive menu
├── Makefile
└── README.md
```

## Requirements

- Linux (uses `sbrk()` and `mmap()`, both POSIX/Linux APIs)
- `g++` with C++17 support (tested with GCC 13)
- `make`

## Build

```bash
cd custom_allocator
make
```

This produces an executable called `allocator_demo`. `make clean` removes
build artifacts.

## Run

**Interactive menu:**
```bash
./allocator_demo
```
You'll get a menu to pick which demo to run (First-Fit, Best-Fit, coalescing,
buddy system, leak detection, or all of them).

**Non-interactive (useful for scripting/testing):**
```bash
./allocator_demo first-fit   # First-Fit allocation + reuse demo
./allocator_demo best-fit    # Best-Fit picks the smallest sufficient hole
./allocator_demo coalesce    # Shows adjacent free blocks merging
./allocator_demo buddy       # Buddy system split/merge demo
./allocator_demo leak        # Leak detector catching un-freed blocks
./allocator_demo all         # Runs everything in sequence
```

## Using the allocator in your own code

```cpp
#include "allocator.h"
#include "buddy_allocator.h"
#include "leak_detector.h"

using namespace calloc_lib;

// Free-list allocator
set_strategy(Strategy::BEST_FIT);      // or Strategy::FIRST_FIT
void* p = my_malloc(128);
my_free(p);

// Buddy allocator
buddy_lib::buddy_init(1 << 20);        // reserve a 1 MB pool
void* q = buddy_lib::buddy_alloc(500);
buddy_lib::buddy_free(q);
buddy_lib::buddy_destroy();

// Leak-tracked allocation (captures file/line automatically)
void* r = LMALLOC(64);
LFREE(r);
leak_lib::report_leaks();              // call at end of program
```

## How each piece works

### Free-list allocator (`allocator.cpp`)
- Every block has a header (`size`, `is_free`, `next`, `prev`) stored right
  before the memory handed to the caller.
- Because `sbrk()` grows the heap contiguously, the header list is naturally
  **in address order**, so a block's `next`/`prev` are also its physical
  neighbors — which makes coalescing a simple O(1) pointer check.
- **First-Fit**: returns the first free block big enough.
- **Best-Fit**: scans the whole list and returns the smallest block that
  still fits, minimizing wasted space per allocation (at the cost of a full
  list scan).
- **Splitting**: if a chosen free block is much bigger than needed, it's
  split into a used part and a new free remainder block.
- **Coalescing**: on `free()`, the block merges with its physical
  neighbours if they're also free, so freed memory doesn't stay fragmented
  into many small unusable holes.
- A double-free is detected (checked via the `is_free` flag) and reported
  instead of corrupting the heap.

### Buddy system (`buddy_allocator.cpp`)
- Reserves one large power-of-two pool via `mmap()`.
- Allocation requests are rounded up to the nearest power-of-two block size
  ("order"). If no free block of that exact order exists, a larger block is
  recursively split in half ("buddies") until the right size is produced.
- On `free()`, the allocator computes the address of the block's buddy using
  the classic XOR trick (`buddy_offset = offset ^ (1 << order)`), and if that
  buddy is also free, merges them into one larger block — repeating until no
  more merges are possible. This is what lets the whole pool recombine back
  into a single free block, as shown in the demo.
- Trade-off vs. the free-list allocator: buddy allocation/free are fast
  (bounded by pool depth, not list length) but can waste more memory to
  internal fragmentation, since every request is rounded up to a power of two.

### Leak detector (`leak_detector.cpp`)
- `LMALLOC(size)` calls `my_malloc()` and records `{size, file, line}` in a
  map keyed by pointer.
- `LFREE(ptr)` removes it from the map, then calls `my_free()`.
- `report_leaks()` prints every allocation still in the map — i.e., every
  block that was never freed, along with where it was allocated.

## Extending the project

Ideas if you want to keep building on this:
- Add a **worst-fit** strategy for comparison.
- Track fragmentation statistics (largest free block vs. total free bytes).
- Add unit tests (e.g. with a lightweight framework or plain `assert()`).
- Wrap `my_malloc`/`my_free` behind `operator new`/`operator delete` to use
  it as your program's global allocator.
- Add thread-per-strategy benchmarks comparing First-Fit, Best-Fit, and Buddy
  under different allocation patterns.
