// ============================================================
// DriverRegistry.cpp
// F1 Pound-for-Pound Rating System
// Author: Nigel Black
//
// Implementation of DriverRegistry. See DriverRegistry.h for
// design notes on data structure choice (unordered_map vs map).
// ============================================================

#include "DriverRegistry.h"
#include <iostream>
#include <algorithm>
#include <iomanip>

// Free all heap-allocated Driver and Constructor objects
DriverRegistry::~DriverRegistry() {
    for (auto& pair : drivers) delete pair.second;
    for (auto& pair : teams)   delete pair.second;
}

// O(1) average lookup. Creates a new Driver at rating 1500 if the ID
// is unseen (rookie), otherwise updates teamId to keep the current team current.
Driver* DriverRegistry::getOrCreateDriver(std::string id, std::string name,
                                           std::string teamId, int currentYear) {
    if (drivers.find(id) == drivers.end()) {
        drivers[id] = new Driver(id, name, teamId);
    } else {
        drivers[id]->teamId = teamId;
    }
    if (currentYear > 0) drivers[id]->lastSeasonRaced = currentYear;
    return drivers[id];
}

// O(1) average lookup. Creates a new Constructor at seasonRating 2000 if unseen.
Constructor* DriverRegistry::getOrCreateTeam(std::string id, std::string name, int currentYear) {
    if (teams.find(id) == teams.end()) {
        teams[id] = new Constructor(id, name);
    }
    if (currentYear > 0) teams[id]->lastSeasonRaced = currentYear;
    teams[id]->updatePeak();
    return teams[id];
}

// Resets all constructor season ratings to 2000 to model the effect of
// major regulation changes that periodically reset the constructor hierarchy.
void DriverRegistry::resetTeamRatings() {
    for (auto& pair : teams) {
        pair.second->seasonRating = 2000.0;
    }
}

// Sorts all drivers by peakRating descending and prints the top 15.
// peakRating is used (not allTimeRating) so a driver's best-ever form
// is reflected regardless of career decline.
void DriverRegistry::printGoatList() const {
    std::vector<Driver*> goatList;
    for (auto& pair : drivers) goatList.push_back(pair.second);

    std::sort(goatList.begin(), goatList.end(), [](Driver* a, Driver* b) {
        return a->peakRating > b->peakRating;
    });

    std::cout << "\n========== FORMULA 1 ALL-TIME GOATS (PEAK RATING) ==========\n";
    std::cout << std::left << std::setw(6) << "Rank"
              << std::setw(25) << "Name"
              << "Peak Elo\n";
    std::cout << "----------------------------------------------------------\n";

    for (int i = 0; i < (int)goatList.size() && i < 15; ++i) {
        std::cout << std::left << std::setw(6) << (i + 1)
                  << std::setw(25) << goatList[i]->name
                  << (int)goatList[i]->peakRating << "\n";
    }
}

// Filters to drivers who raced in currentYear or currentYear-1, then sorts
// by allTimeRating (live Elo, not peak) to reflect current form.
// The -1 window handles mid-season data where the current year is incomplete.
void DriverRegistry::printActiveDriverRankings(int currentYear) const {
    std::vector<Driver*> activeDrivers;
    for (auto& pair : drivers) {
        if (pair.second->lastSeasonRaced >= currentYear - 1 && pair.second->raceCount > 0) {
            activeDrivers.push_back(pair.second);
        }
    }

    std::sort(activeDrivers.begin(), activeDrivers.end(), [](Driver* a, Driver* b) {
        return a->allTimeRating > b->allTimeRating;
    });

    std::cout << "\n========== ACTIVE DRIVER POWER RANKINGS ==========\n";
    std::cout << std::left << std::setw(6) << "Rank"
              << std::setw(25) << "Name"
              << std::setw(20) << "Team"
              << std::setw(14) << "Current Elo"
              << "Peak Elo\n";
    std::cout << "-------------------------------------------------------------------\n";

    for (int i = 0; i < (int)activeDrivers.size(); ++i) {
        Driver* d = activeDrivers[i];
        std::string teamName = d->teamId;
        if (teams.find(d->teamId) != teams.end()) {
            teamName = teams.at(d->teamId)->name;
        }
        std::cout << std::left << std::setw(6) << (i + 1)
                  << std::setw(25) << d->name
                  << std::setw(20) << teamName
                  << std::setw(14) << (int)d->allTimeRating
                  << (int)d->peakRating << "\n";
    }

    if (activeDrivers.empty()) {
        std::cout << "  No active driver data found. Run a sync first.\n";
    }
}

// Filters to constructors active in currentYear or currentYear-1,
// sorted by seasonRating (current era Elo, not all-time peak).
void DriverRegistry::printTeamRankings(int currentYear) const {
    std::vector<Constructor*> activeTeams;
    for (auto& pair : teams) {
        if (pair.second->lastSeasonRaced >= currentYear - 1) {
            activeTeams.push_back(pair.second);
        }
    }

    std::sort(activeTeams.begin(), activeTeams.end(), [](Constructor* a, Constructor* b) {
        return a->seasonRating > b->seasonRating;
    });

    std::cout << "\n========== CURRENT SEASON CONSTRUCTOR POWER RANKINGS ==========\n";
    std::cout << std::left << std::setw(6) << "Rank"
              << std::setw(30) << "Constructor"
              << "Season Elo\n";
    std::cout << "------------------------------------------------------\n";

    for (int i = 0; i < (int)activeTeams.size(); ++i) {
        std::cout << std::left << std::setw(6) << (i + 1)
                  << std::setw(30) << activeTeams[i]->name
                  << (int)activeTeams[i]->seasonRating << "\n";
    }

    if (activeTeams.empty()) {
        std::cout << "  No active constructor data found. Run a sync first.\n";
    }
}

// All constructors ever, sorted by peakSeasonRating. Shows lastSeasonRaced
// so you can contextualise when each team's peak occurred.
void DriverRegistry::printAllTimeTeamRankings() const {
    std::vector<Constructor*> teamList;
    for (auto& pair : teams) teamList.push_back(pair.second);

    std::sort(teamList.begin(), teamList.end(), [](Constructor* a, Constructor* b) {
        return a->peakSeasonRating > b->peakSeasonRating;
    });

    std::cout << "\n========== ALL-TIME CONSTRUCTOR RANKINGS (PEAK SEASON ELO) ==========\n";
    std::cout << std::left << std::setw(6) << "Rank"
              << std::setw(30) << "Constructor"
              << std::setw(16) << "Peak Season Elo"
              << "Last Season\n";
    std::cout << "--------------------------------------------------------------------\n";

    for (int i = 0; i < (int)teamList.size() && i < 15; ++i) {
        std::cout << std::left << std::setw(6) << (i + 1)
                  << std::setw(30) << teamList[i]->name
                  << std::setw(16) << (int)teamList[i]->peakSeasonRating
                  << teamList[i]->lastSeasonRaced << "\n";
    }

    if (teamList.empty()) {
        std::cout << "  No constructor data found. Run a sync first.\n";
    }
}
