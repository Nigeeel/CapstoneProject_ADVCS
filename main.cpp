// ============================================================
// main.cpp
// F1 Pound-for-Pound Rating System
// Author: Nigel Black
//
// Entry point and application controller. Manages the main
// menu loop, coordinates the live API sync pipeline, and
// delegates display and benchmarking to their respective
// subsystems.
//
// Sync pipeline per season year:
//   1. Download JSON from jolpi.ca/ergast if not cached
//   2. Parse race results via F1Parser
//   3. Process each race through RatingEngine (Elo math)
//   4. Award WDC bonus to the season champion
//   5. Decadal constructor rating reset for era parity
// ============================================================

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <unordered_map>
#include "DriverRegistry.h"
#include "RatingEngine.h"
#include "F1Parser.h"
#include "Benchmarker.h"
#include "MapBenchmark.h"

// Global benchmarker — persists across menu interactions so the
// report remains available after a sync completes.
Benchmarker benchmarker;

// -------------------------------------------------------
// syncWithAPI
// Downloads and processes every F1 season from 1950 to
// the current year. Resumes from the last saved position
// in progress.txt so interrupted syncs don't restart from
// scratch. Measures per-race latency for the benchmark report.
// -------------------------------------------------------
void syncWithAPI(DriverRegistry& registry, RatingEngine& engine, F1Parser& parser) {
    time_t t = time(NULL);
    tm* timePtr = localtime(&t);
    int currentYear = timePtr->tm_year + 1900;

    std::cout << "\n[SYSTEM] Starting Live API Sync..." << std::endl;

    int lastYearProcessed = 1950;
    int lastRaceProcessed = 0;

    // Resume from saved progress if available
    std::ifstream progressFile("progress.txt");
    if (progressFile.is_open()) {
        progressFile >> lastYearProcessed >> lastRaceProcessed;
        progressFile.close();
    }

    benchmarker.startSync();

    for (int year = lastYearProcessed; year <= currentYear; ++year) {
        std::unordered_map<std::string, int> seasonStandings;
        std::string filename = "season_" + std::to_string(year) + ".json";

        // --- Step 1: Download season JSON if not already cached ---
        std::ifstream checkFile(filename);
        bool needsDownload = !checkFile.good();
        checkFile.close();

        if (needsDownload) {
            std::cout << "[FETCH] Year: " << year << std::endl;

            // Write to .tmp first — prevents a failed download from leaving
            // a corrupt file that silently passes the content.length() check.
            std::string tempFile = filename + ".tmp";
            std::string command = "curl -k -L -s -A \"Mozilla/5.0\" --connect-timeout 15 --max-time 30 "
                                  "https://api.jolpi.ca/ergast/f1/" + std::to_string(year) + "/results.json"
                                  " -o " + tempFile + " -w \"%{http_code}\"";

            FILE* pipe = popen(command.c_str(), "r");
            std::string httpCode = "000";
            if (pipe) {
                char buf[8] = {};
                if (fgets(buf, sizeof(buf), pipe)) httpCode = buf;
                pclose(pipe);
            }

            if (httpCode.find("200") != std::string::npos) {
                std::rename(tempFile.c_str(), filename.c_str());
            } else {
                std::cerr << "[FETCH] Failed for " << year << " (HTTP " << httpCode
                          << "). Will retry next sync." << std::endl;
                std::remove(tempFile.c_str());
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        // --- Step 2: Read cached JSON into memory ---
        std::ifstream ifs(filename);
        if (!ifs.is_open()) {
            std::cerr << "[ERROR] Could not open " << filename << std::endl;
            continue;
        }
        std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
        ifs.close();

        // Sanity check — skip files that are clearly malformed or empty responses
        if (content.length() < 200) continue;

        // --- Step 3: Parse and process each race in the season ---
        auto seasonRaces = parser.parseSeasonResults(content, registry, year);
        bool isYearPast = (year < currentYear);

        for (int i = 0; i < (int)seasonRaces.size(); ++i) {
            // Skip races already processed on a previous interrupted run
            if (year == lastYearProcessed && i < lastRaceProcessed) continue;

            std::cout << "[PROCESS] " << year << " - " << seasonRaces[i].raceName << std::endl;

            // Time each race session individually for the benchmark report
            benchmarker.startSession();
            engine.processSession(seasonRaces[i].grid, registry.getAllTeams(), false);
            benchmarker.endSession((int)seasonRaces[i].grid.size());

            // Accumulate championship points using the standard F1 points table
            int ptsTable[] = {25, 18, 15, 12, 10, 8, 6, 4, 2, 1};
            for (int r = 0; r < 10 && r < (int)seasonRaces[i].grid.size(); ++r) {
                seasonStandings[seasonRaces[i].grid[r]->id] += ptsTable[r];
            }

            // Persist progress so we can resume if the sync is interrupted
            std::ofstream saveFile("progress.txt");
            saveFile << year << " " << (i + 1);
            saveFile.close();
        }

        // --- Step 4: Award WDC bonus (completed seasons only) ---
        if (isYearPast && !seasonStandings.empty()) {
            std::string champId = "";
            int maxPts = -1;
            for (auto const& [id, pts] : seasonStandings) {
                if (pts > maxPts) { maxPts = pts; champId = id; }
            }

            if (!champId.empty()) {
                // Look up directly — the champion must already exist since they scored
                // points this season. Calling getOrCreateDriver with empty strings would
                // risk creating a nameless ghost driver entry if the ID somehow failed.
                const auto& allDrivers = registry.getAllDrivers();
                auto it = allDrivers.find(champId);
                if (it == allDrivers.end()) {
                    std::cerr << "[WDC] WARNING: Champion ID '" << champId
                              << "' not found in registry. Skipping bonus." << std::endl;
                } else {
                    Driver* wdc = it->second;
                    // Bonus scales with existing title count — each additional championship
                    // is worth 15 more Elo points to reflect the compounding difficulty
                    // of sustaining dominance across multiple seasons.
                    double scalingBonus = 60.0 + (wdc->titlesWon * 15.0);
                    wdc->allTimeRating += scalingBonus;
                    wdc->titlesWon++;
                    wdc->updatePeak();
                    std::cout << "[WDC] " << year << " Champion: " << wdc->name
                              << " (Title #" << wdc->titlesWon
                              << ", +" << (int)scalingBonus << " Elo)" << std::endl;
                }
            }
        } else if (year == currentYear) {
            std::cout << "[INFO] " << year << " season in progress. WDC bonus pending." << std::endl;
        }

        // --- Step 5: Decadal constructor reset ---
        // Every 10 years, team season ratings reset to the 2000 baseline. This models
        // the effect of major technical regulation changes that periodically reset the
        // constructor hierarchy (e.g. ground effect ban 1983, FRIC ban 2014, 2022 regs).
        if (year % 10 == 0) registry.resetTeamRatings();
    }

    benchmarker.endSync();
    std::cout << "\n[SUCCESS] Sync Complete. Select option 6 to view benchmark results." << std::endl;
}

// -------------------------------------------------------
// main
// Presents the interactive menu and routes user selections
// to the appropriate registry or benchmarker functions.
// -------------------------------------------------------
int main() {
    runMapComparison();
    DriverRegistry registry;
    RatingEngine engine;
    F1Parser parser;

    int choice = 0;
    while (choice != 8) {
        std::cout << "\n====================================\n";
        std::cout << "   F1 POUND-FOR-POUND RATING SYSTEM \n";
        std::cout << "====================================\n";
        std::cout << "1. Sync with Live API (Process New Races)\n";
        std::cout << "2. View All-Time GOAT Leaderboard (Peak Driver Rating)\n";
        std::cout << "3. View Active Driver Power Rankings (Current Rating)\n";
        std::cout << "4. View Current Season Constructor Rankings\n";
        std::cout << "5. View All-Time Constructor Rankings (Peak Season Elo)\n";
        std::cout << "6. View Performance Benchmark Report\n";
        std::cout << "7. Export Benchmark Data to CSV\n";
        std::cout << "8. Exit\n";
        std::cout << "Selection: ";

        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(1000, '\n');
            continue;
        }

        time_t t = time(NULL);
        tm* timePtr = localtime(&t);
        int currentYear = timePtr->tm_year + 1900;

        if      (choice == 1) syncWithAPI(registry, engine, parser);
        else if (choice == 2) registry.printGoatList();
        else if (choice == 3) registry.printActiveDriverRankings(currentYear);
        else if (choice == 4) registry.printTeamRankings(currentYear);
        else if (choice == 5) registry.printAllTimeTeamRankings();
        else if (choice == 6) benchmarker.printReport();
        else if (choice == 7) benchmarker.exportCSV("benchmark_data.csv");
        else if (choice == 8) break;
    }

    return 0;
}
