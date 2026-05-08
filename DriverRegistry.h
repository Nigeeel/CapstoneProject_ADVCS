// ============================================================
// DriverRegistry.h
// F1 Pound-for-Pound Rating System
// Author: Nigel Black
//
// Central store for all Driver and Constructor objects.
// Uses std::unordered_map<string, T*> for both collections,
// giving O(1) average-case lookup by ID string. This is
// critical during the sync loop where getOrCreateDriver is
// called once per finishing position across ~1,100 races —
// a std::map alternative would cost O(log n) per call,
// roughly 10x more comparisons at full dataset size.
//
// Ownership: DriverRegistry owns all Driver* and Constructor*
// pointers and deletes them in its destructor.
// ============================================================

#ifndef CAPSTONEPROJECT_ADVCS_DRIVERREGISTRY_H
#define CAPSTONEPROJECT_ADVCS_DRIVERREGISTRY_H

#include <unordered_map>
#include <string>
#include "Models.h"

class DriverRegistry {
private:
    std::unordered_map<std::string, Driver*>      drivers;
    std::unordered_map<std::string, Constructor*> teams;

public:
    // Returns existing driver by ID or creates a new one.
    // Updates teamId on every call so the driver's current team stays current.
    // currentYear stamps lastSeasonRaced for active-driver filtering.
    Driver* getOrCreateDriver(std::string id, std::string name, std::string teamId, int currentYear = 0);

    // Returns existing constructor by ID or creates a new one.
    // currentYear stamps lastSeasonRaced for active-constructor filtering.
    Constructor* getOrCreateTeam(std::string id, std::string name, int currentYear = 0);

    // Read-only access — used by the WDC bonus logic to safely look up
    // a champion without risking accidental ghost-driver creation.
    const std::unordered_map<std::string, Driver*>& getAllDrivers() const { return drivers; }

    // Const and non-const overloads — compiler picks based on context.
    // RatingEngine needs mutable access to update seasonRating on the fly.
    const std::unordered_map<std::string, Constructor*>& getAllTeams() const { return teams; }
    std::unordered_map<std::string, Constructor*>&       getAllTeams()       { return teams; }

    // Resets all constructor seasonRatings to 2000 baseline.
    // Called every 10 years to model regulation-era resets.
    void resetTeamRatings();

    // --- Display functions ---
    void printGoatList()                         const; // All-time peak driver Elo, top 15
    void printActiveDriverRankings(int year)     const; // Current Elo, active drivers only
    void printTeamRankings(int currentYear)      const; // Current season Elo, active teams only
    void printAllTimeTeamRankings()              const; // Peak season Elo, all constructors ever

    ~DriverRegistry();
};

#endif
