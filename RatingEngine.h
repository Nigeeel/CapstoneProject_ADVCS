// ============================================================
// RatingEngine.h
// F1 Pound-for-Pound Rating System
// Author: Nigel Black
//
// Processes a single race session and updates driver/team Elo
// ratings. Core algorithm components:
//
//   Season Density Scaling
//     Early seasons (pre-1960s) had as few as 7 races vs 24
//     today. A raw Elo system would over-reward drivers from
//     short seasons. Density scaling normalises each race's
//     weight so total seasonal Elo movement is comparable
//     across all eras.
//
//   Expected Grid (O(n log n) sort)
//     Drivers are sorted by a blended potential score:
//       potential = 0.5 * driverElo + 0.5 * teamSeasonElo
//     This separates driver skill from car advantage —
//     a great driver in a slow car is still expected to
//     finish ahead of their predicted position.
//
//   Career Weight (arctan curve)
//     Rookies gain/lose less per race. Rating confidence
//     increases with race count, asymptotically approaching
//     full weight at ~120 races. Prevents a single lucky
//     result from inflating a new driver permanently.
//
//   Tanh Smoothing
//     Raw position delta is passed through tanh to compress
//     extreme outliers (e.g. winning from P20 due to a
//     safety car). Keeps the rating system stable.
//
//   Podium Bonus
//     Fixed bonus for P1/P2/P3 on top of the delta movement.
//     Reflects the disproportionate significance of podiums.
//
//   Teammate Duel (standard Elo)
//     Head-to-head Elo exchange between teammates. Isolates
//     driver skill by controlling for car — two drivers in
//     the same machinery provide a clean comparison signal.
// ============================================================

#ifndef CAPSTONEPROJECT_ADVCS_RATINGENGINE_H
#define CAPSTONEPROJECT_ADVCS_RATINGENGINE_H

#include <vector>
#include <unordered_map>
#include "Models.h"

class RatingEngine {
public:
    // Processes one race session, updating allTimeRating and seasonRating
    // for all drivers and their constructors in the grid.
    // grid    : drivers in finishing order (index 0 = winner)
    // teams   : mutable map — constructor seasonRating updated in place
    // isQualifying : uses a lower K-factor (not currently used in sync loop)
    void processSession(std::vector<Driver*>& grid,
                        std::unordered_map<std::string, Constructor*>& teams,
                        bool isQualifying);

private:
    const double K_RACE  = 60.0; // Base K-factor for race sessions
    const double K_QUALY = 20.0; // Base K-factor for qualifying (reserved)
};

#endif
