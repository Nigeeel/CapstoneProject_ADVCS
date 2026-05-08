// ============================================================
// Models.h
// F1 Pound-for-Pound Rating System
// Author: Nigel Black
//
// Plain data structs shared across all subsystems.
// Keeping models in a dedicated header avoids circular
// dependencies between DriverRegistry, F1Parser, and
// RatingEngine.
//
// Constructor — represents an F1 team in a given era.
//   seasonRating    : Elo that resets each decade to model
//                     regulation-era parity resets.
//   peakSeasonRating: Best season Elo ever recorded, used
//                     for the all-time constructor ranking.
//   lastSeasonRaced : Stamps the most recent year the team
//                     appeared in race data, used to filter
//                     retired constructors from active views.
//
// Driver — represents a driver across their entire career.
//   allTimeRating   : Live Elo updated after every race.
//   peakRating      : Best allTimeRating ever reached.
//   raceCount       : Gates the career weight function in
//                     RatingEngine — rookies gain/lose less.
//   titlesWon       : Drives the scaling WDC bonus.
//   lastSeasonRaced : Same purpose as Constructor field.
// ============================================================

#ifndef MODELS_H
#define MODELS_H

#include <string>

struct Constructor {
    std::string id;
    std::string name;
    double seasonRating;      // Resets each decade — current era strength
    double peakSeasonRating;  // All-time best season Elo
    int lastSeasonRaced = 0;  // Filters retired constructors from active views

    Constructor(std::string _id, std::string _name)
        : id(_id), name(_name), seasonRating(2000.0), peakSeasonRating(2000.0), lastSeasonRaced(0) {}

    // Call after any seasonRating update to keep peakSeasonRating current
    void updatePeak() {
        if (seasonRating > peakSeasonRating) peakSeasonRating = seasonRating;
    }
};

struct Driver {
    std::string id;
    std::string name;
    double allTimeRating;     // Live Elo — updated after every race
    double peakRating;        // Best allTimeRating ever reached
    std::string teamId;       // Current or most recent constructor
    int raceCount = 0;        // Career race count — gates career weight in RatingEngine
    int titlesWon = 0;        // WDC count — scales the championship Elo bonus
    int lastSeasonRaced = 0;  // Filters retired drivers from active views

    Driver(std::string _id, std::string _name, std::string _team)
        : id(_id), name(_name), teamId(_team),
          allTimeRating(1500.0), peakRating(1500.0),
          raceCount(0), titlesWon(0), lastSeasonRaced(0) {}

    // Call after any allTimeRating update to keep peakRating current
    void updatePeak() {
        if (allTimeRating > peakRating) peakRating = allTimeRating;
    }
};

#endif
