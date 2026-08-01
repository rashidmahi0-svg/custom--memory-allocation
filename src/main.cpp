#include <iostream>
#include <string>

#include "allocator.h"
#include "buddy_allocator.h"
#include "leak_detector.h"

using namespace calloc_lib;
using namespace buddy_lib;

void demo_first_fit() {
    std::cout << "\n=== First-Fit Allocator Demo ===\n";
    set_strategy(Strategy::FIRST_FIT);

    void* a = my_malloc(64);
    void* b = my_malloc(128);
    void* c = my_malloc(32);
    std::cout << "Allocated a(64), b(128), c(32)\n";
    print_heap_map();

    my_free(b);
    std::cout << "\nFreed b -> leaves a hole\n";
    print_heap_map();

    void* d = my_malloc(100);
    std::cout << "\nAllocated d(100) -> first-fit reuses b's hole\n";
    print_heap_map();

    my_free(a); my_free(c); my_free(d);
    std::cout << "\nFreed everything -> adjacent free blocks coalesce\n";
    print_heap_map();
}

void demo_best_fit() {
    std::cout << "\n=== Best-Fit Allocator Demo ===\n";
    set_strategy(Strategy::BEST_FIT);

    void* a = my_malloc(200);
    void* b = my_malloc(50);
    void* c = my_malloc(300);
    my_free(a);
    my_free(c);
    std::cout << "Freed a(~200) and c(~300); two holes of different sizes now exist\n";

    void* d = my_malloc(80);
    std::cout << "Allocated d(80) -> best-fit should pick the smaller (~200) hole\n";
    print_heap_map();

    my_free(b); my_free(d);
}

void demo_coalescing() {
    std::cout << "\n=== Coalescing / Fragmentation Demo ===\n";
    set_strategy(Strategy::FIRST_FIT);
    void* blocks[5];
    for (int i = 0; i < 5; i++) blocks[i] = my_malloc(64);
    print_heap_map();

    std::cout << "\nFreeing blocks 1, 2, 3 (physically adjacent)...\n";
    my_free(blocks[1]);
    my_free(blocks[2]);
    my_free(blocks[3]);
    print_heap_map();
    std::cout << "Notice the three adjacent free blocks merged into one.\n";

    my_free(blocks[0]);
    my_free(blocks[4]);
}

void demo_buddy() {
    std::cout << "\n=== Buddy System Demo ===\n";
    buddy_init(1 << 16); // 64 KB pool

    void* a = buddy_alloc(1000);
    void* b = buddy_alloc(3000);
    void* c = buddy_alloc(500);
    print_buddy_map();

    std::cout << "\nFreeing b...\n";
    buddy_free(b);
    print_buddy_map();

    std::cout << "\nFreeing a and c -> buddies should merge back into larger blocks\n";
    buddy_free(a);
    buddy_free(c);
    print_buddy_map();

    buddy_destroy();
}

void demo_leak_detection() {
    std::cout << "\n=== Leak Detection Demo ===\n";
    void* a = LMALLOC(48);
    void* b = LMALLOC(96);
    void* c = LMALLOC(16);
    (void)a;

    LFREE(b);
    std::cout << "Allocated 3 blocks, freed only 1 -> expect 2 leaks reported\n";
    leak_lib::report_leaks();

    LFREE(a);
    LFREE(c);
    std::cout << "\nFreed the remaining blocks:\n";
    leak_lib::report_leaks();
}

void run_all() {
    demo_first_fit();
    demo_best_fit();
    demo_coalescing();
    demo_buddy();
    demo_leak_detection();
}

int main(int argc, char** argv) {
    std::cout << "Custom Memory Allocator - Demo Suite\n";
    std::cout << "======================================\n";

    if (argc > 1) {
        std::string arg = argv[1];
        if      (arg == "first-fit") demo_first_fit();
        else if (arg == "best-fit")  demo_best_fit();
        else if (arg == "coalesce")  demo_coalescing();
        else if (arg == "buddy")     demo_buddy();
        else if (arg == "leak")      demo_leak_detection();
        else if (arg == "all")       run_all();
        else std::cout << "Unknown option: " << arg
                        << "\nValid options: first-fit | best-fit | coalesce | buddy | leak | all\n";
        return 0;
    }

    while (true) {
        std::cout << "\nChoose a demo:\n"
                  << "  1) First-Fit allocation\n"
                  << "  2) Best-Fit allocation\n"
                  << "  3) Coalescing / fragmentation\n"
                  << "  4) Buddy system\n"
                  << "  5) Leak detection\n"
                  << "  6) Run all\n"
                  << "  0) Exit\n"
                  << "> ";
        int choice;
        if (!(std::cin >> choice)) break;
        switch (choice) {
            case 1: demo_first_fit();     break;
            case 2: demo_best_fit();      break;
            case 3: demo_coalescing();    break;
            case 4: demo_buddy();         break;
            case 5: demo_leak_detection();break;
            case 6: run_all();            break;
            case 0: return 0;
            default: std::cout << "Invalid choice\n";
        }
    }
    return 0;
}
