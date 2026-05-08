// ============================================================
// MapBenchmark.h
// F1 Pound-for-Pound Rating System
// Author: Nigel Black
//
// Standalone micro-benchmark comparing std::unordered_map vs
// std::map for string-keyed driver ID lookups — the exact
// access pattern used in DriverRegistry during the sync loop.
//
// Call runMapComparison() once from main before the menu loop.
// Results feed directly into the benchmark slide justification.
// ============================================================

#ifndef CAPSTONEPROJECT_ADVCS_MAPBENCHMARK_H
#define CAPSTONEPROJECT_ADVCS_MAPBENCHMARK_H

#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>

// Simulates the actual driver ID strings used by the Ergast API
static std::vector<std::string> generateDriverIds(int count) {
    // Real Ergast driver IDs look like: "max_verstappen", "hamilton", "leclerc"
    // We generate plausible synthetic ones for a fair benchmark
    std::vector<std::string> ids;
    std::vector<std::string> firstNames = {"max", "lewis", "charles", "sergio", "george",
                                            "lando", "carlos", "fernando", "sebastian", "kimi",
                                            "nico", "jenson", "michael", "ayrton", "alain"};
    std::vector<std::string> lastNames  = {"verstappen", "hamilton", "leclerc", "perez", "russell",
                                            "norris", "sainz", "alonso", "vettel", "raikkonen",
                                            "rosberg", "button", "schumacher", "senna", "prost"};

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> firstDist(0, (int)firstNames.size() - 1);
    std::uniform_int_distribution<int> lastDist(0,  (int)lastNames.size()  - 1);
    std::uniform_int_distribution<int> numDist(1, 99);

    for (int i = 0; i < count; ++i) {
        ids.push_back(firstNames[firstDist(rng)] + "_" + lastNames[lastDist(rng)]
                      + "_" + std::to_string(numDist(rng)));
    }
    return ids;
}

inline void runMapComparison() {
    const int DRIVER_COUNT   = 1100;   // Approximate real registry size after full sync
    const int LOOKUP_COUNT   = 22000;  // ~20 drivers/race × ~1100 races
    const int TRIALS         = 5;      // Average over multiple runs to reduce noise

    auto ids = generateDriverIds(DRIVER_COUNT);

    // --- Pre-populate both maps with the same data ---
    std::unordered_map<std::string, int> umap;
    std::map<std::string, int>           omap;

    for (int i = 0; i < DRIVER_COUNT; ++i) {
        umap[ids[i]] = i;
        omap[ids[i]] = i;
    }

    // Build a lookup sequence (random access, same keys repeated — mirrors real usage)
    std::mt19937 rng(99);
    std::uniform_int_distribution<int> idxDist(0, DRIVER_COUNT - 1);
    std::vector<std::string> lookupSequence;
    lookupSequence.reserve(LOOKUP_COUNT);
    for (int i = 0; i < LOOKUP_COUNT; ++i) {
        lookupSequence.push_back(ids[idxDist(rng)]);
    }

    // --- Benchmark unordered_map ---
    double umapTotalMs = 0.0;
    volatile int sink = 0; // Prevent the compiler from optimising away the lookups
    for (int t = 0; t < TRIALS; ++t) {
        auto start = std::chrono::high_resolution_clock::now();
        for (auto& key : lookupSequence) {
            auto it = umap.find(key);
            if (it != umap.end()) sink += it->second;
        }
        auto end = std::chrono::high_resolution_clock::now();
        umapTotalMs += std::chrono::duration<double, std::milli>(end - start).count();
    }
    double umapAvgMs = umapTotalMs / TRIALS;

    // --- Benchmark std::map ---
    double omapTotalMs = 0.0;
    for (int t = 0; t < TRIALS; ++t) {
        auto start = std::chrono::high_resolution_clock::now();
        for (auto& key : lookupSequence) {
            auto it = omap.find(key);
            if (it != omap.end()) sink += it->second;
        }
        auto end = std::chrono::high_resolution_clock::now();
        omapTotalMs += std::chrono::duration<double, std::milli>(end - start).count();
    }
    double omapAvgMs = omapTotalMs / TRIALS;

    double speedup = (umapAvgMs > 0) ? (omapAvgMs / umapAvgMs) : 0.0;

    // --- Print results ---
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "     DATA STRUCTURE COMPARISON: unordered_map vs std::map      \n";
    std::cout << "================================================================\n\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  Setup: " << DRIVER_COUNT << " drivers, " << LOOKUP_COUNT
              << " lookups, averaged over " << TRIALS << " trials\n\n";
    std::cout << "  std::unordered_map  avg time : " << umapAvgMs << " ms   (O(1) avg)\n";
    std::cout << "  std::map            avg time : " << omapAvgMs << " ms   (O(log n))\n";
    std::cout << std::setprecision(2);
    std::cout << "  Speedup                      : " << speedup << "x faster\n\n";
    std::cout << "  Theoretical O(log n) factor at n=1100: ~" << std::setprecision(1)
              << std::log2(1100.0) << " comparisons per lookup vs ~1 for hash\n";
    std::cout << "\n================================================================\n";

    (void)sink;
}

#endif //CAPSTONEPROJECT_ADVCS_MAPBENCHMARK_H
