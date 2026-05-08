# F1 Pound-for-Pound Rating System
**Author:** Nigel Black  
**Course:** Advanced Data Structures — Capstone Project

A real-time C++ application that pulls live race result data from the Jolpi/Ergast F1 API and computes Elo-based power ratings for every driver and constructor in Formula 1 history (1950–present).

---

## What It Does

- **Syncs live from the API** — downloads every season's race results, caches them locally, and resumes interrupted syncs automatically
- **Rates drivers** using a multi-component Elo system: blended car/driver potential, tanh-smoothed position deltas, arctan career weighting, podium bonuses, and head-to-head teammate duels
- **Rates constructors** using a field-relative iRating-style Elo exchange — every team that raced is compared against every other team using average driver finishing position
- **Tracks peaks separately** from live ratings, so all-time GOAT lists reflect a driver's best-ever form, not career decline
- **Resets constructor era ratings** every decade to model the effect of major regulation changes
- **Benchmarks itself** — measures per-race latency, total throughput, and compares against a naive O(n²) baseline

---

## Setup & Build

### Requirements
- C++20 or later
- CMake 3.x+
- `curl` available on your system PATH (used for API downloads)
- Internet connection for the initial sync (subsequent runs use local cache)

### Build Instructions

```bash
git clone <your-repo-url>
cd <repo-folder>
mkdir build && cd build
cmake ..
make
./CapstoneProject_ADVCS
```

The `json.hpp` single-header library (nlohmann/json) must be present in the project root. It is included in the repository.

---

## Usage

On launch you'll see the main menu:

```
====================================
   F1 POUND-FOR-POUND RATING SYSTEM
====================================
1. Sync with Live API (Process New Races)
2. View All-Time GOAT Leaderboard (Peak Driver Rating)
3. View Active Driver Power Rankings (Current Rating)
4. View Current Season Constructor Rankings
5. View All-Time Constructor Rankings (Peak Season Elo)
6. View Performance Benchmark Report
7. Exit
```

**First run:** Select option 1 to sync. This downloads ~75 seasons of race data from `api.jolpi.ca/ergast` and processes every race. Expect it to take a few minutes. Progress is saved to `progress.txt` so you can safely interrupt and resume.

**Subsequent runs:** Cached season JSON files are stored in the working directory (`season_YYYY.json`). Only new seasons will be downloaded.

---

## Data Source

All race result data comes from the **Jolpi Ergast F1 API**:  
`https://api.jolpi.ca/ergast/f1/{year}/results.json`

The API returns results in Ergast MRData envelope format, parsed by `F1Parser`.

---

## Project Structure

```
├── main.cpp           — Entry point, menu loop, sync pipeline
├── Models.h           — Driver and Constructor data structs
├── DriverRegistry.h/cpp — Central store for all entities (unordered_map)
├── RatingEngine.h/cpp — All Elo math: driver updates + constructor updates
├── F1Parser.h/cpp     — JSON parsing via nlohmann/json
├── Benchmarker.h      — Per-race timing and throughput reporting
├── json.hpp           — nlohmann/json single-header library
└── CMakeLists.txt
```

---

## Data Structure Decisions

### `std::unordered_map` for Driver and Constructor storage
The registry uses `unordered_map<string, Driver*>` and `unordered_map<string, Constructor*>` for O(1) average-case lookup by ID string. `getOrCreateDriver` is called once per finishing position across ~1,100 races and ~20 drivers per race — roughly 22,000 lookups during a full sync. A `std::map` alternative would cost O(log n) per call; with ~1,100 drivers in the registry that's approximately 10 comparisons per lookup vs. 1.

### `std::vector` for per-race grid processing
Race grids are processed as vectors ordered by finishing position. The expected-grid sort is O(n log n) where n ≤ 26 (max grid size), making it effectively constant. The all-pairs constructor Elo exchange is O(t²) where t = number of unique teams per race (≤ 10), also effectively constant.

---

## Algorithm Overview

Each race session runs through five steps in `RatingEngine::processSession`:

1. **Season density scaling** — K-factor reduced for early-era small grids
2. **Blended potential sort** — predicted finishing order = 50% driver Elo + 50% team Elo, sorted O(n log n)
3. **Driver rating update** — tanh-smoothed position delta × career weight + podium bonus + teammate duel
4. **Constructor rating update** — all-pairs Elo exchange between every team that raced, using average driver finishing position as the outcome signal
5. **Peak tracking** — `updatePeak()` called on every modified entity
