// ============================================================
// F1Parser.h
// F1 Pound-for-Pound Rating System
// Author: Nigel Black
//
// Parses season result JSON from the Jolpi/Ergast F1 API
// into RaceResults structs consumable by RatingEngine.
//
// Expected JSON structure (Ergast MRData envelope):
//   MRData -> RaceTable -> Races[] -> Results[]
//     Results[i].Driver.driverId     (string ID)
//     Results[i].Driver.familyName   (display name)
//     Results[i].Constructor.constructorId
//     Results[i].Constructor.name
//
// Drivers in RaceResults.grid are ordered by finishing
// position (index 0 = winner) as returned by the API.
//
// Dependency: nlohmann/json single-header library (json.hpp)
// ============================================================

#ifndef CAPSTONEPROJECT_ADVCS_F1PARSER_H
#define CAPSTONEPROJECT_ADVCS_F1PARSER_H

#include <string>
#include <vector>
#include "Models.h"
#include "DriverRegistry.h"
#include "json.hpp"

using json = nlohmann::json;

// Holds the results of a single race — name for display,
// grid ordered by finishing position for RatingEngine.
struct RaceResults {
    std::string raceName;
    std::vector<Driver*> grid; // Index 0 = race winner
};

class F1Parser {
public:
    // Parses a full season JSON string and returns one RaceResults per race.
    // Registers any new drivers and constructors with the DriverRegistry.
    // year is passed through so lastSeasonRaced is stamped on each entity.
    std::vector<RaceResults> parseSeasonResults(const std::string& jsonData,
                                                 DriverRegistry& registry,
                                                 int year);
};

#endif
