// ============================================================
// F1Parser.cpp
// F1 Pound-for-Pound Rating System
// Author: Nigel Black
//
// See F1Parser.h for JSON structure documentation.
// ============================================================

#include "F1Parser.h"
#include <iostream>

std::vector<RaceResults> F1Parser::parseSeasonResults(const std::string& jsonData,
                                                       DriverRegistry& registry,
                                                       int year) {
    std::vector<RaceResults> seasonRaces;

    try {
        auto data = json::parse(jsonData);

        // Navigate Ergast envelope: MRData -> RaceTable -> Races array
        auto racesJson = data["MRData"]["RaceTable"]["Races"];

        for (auto& raceItem : racesJson) {
            RaceResults currentRace;
            currentRace.raceName = raceItem.value("raceName", "Unknown GP");

            // Results array is already ordered by finishing position (P1 first)
            auto resultsArray = raceItem["Results"];
            for (auto& result : resultsArray) {
                std::string dId   = result["Driver"]["driverId"];
                std::string dName = result["Driver"]["familyName"];
                std::string tId   = result["Constructor"]["constructorId"];
                std::string tName = result["Constructor"]["name"];

                // Register with the registry (creates on first appearance, updates team otherwise).
                // Passing year stamps lastSeasonRaced so active-filtering works correctly.
                registry.getOrCreateTeam(tId, tName, year);
                Driver* d = registry.getOrCreateDriver(dId, dName, tId, year);

                // Add to grid in finishing order for RatingEngine to process
                currentRace.grid.push_back(d);
            }
            seasonRaces.push_back(currentRace);
        }
    } catch (const std::exception& e) {
        std::cerr << "[PARSER] JSON error for year " << year << ": " << e.what() << std::endl;
    }

    return seasonRaces;
}
