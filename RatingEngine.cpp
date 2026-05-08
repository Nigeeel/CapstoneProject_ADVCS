// ============================================================
// RatingEngine.cpp
// F1 Pound-for-Pound Rating System
// Author: Nigel Black
//
// See RatingEngine.h for full algorithm documentation.
// ============================================================

#include "RatingEngine.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void RatingEngine::processSession(std::vector<Driver*>& grid,
                                  std::unordered_map<std::string, Constructor*>& teams,
                                  bool isQualifying) {

    if (grid.empty()) return;

    // --- Season Density Scaling ---
    // Early-era grids (< 15 drivers) ran far fewer races per season.
    // Scaling K down prevents 1950s drivers from disproportionately
    // accumulating Elo from a low race count inflating each result.
    double seasonDensity = (grid.size() < 15) ? 0.4 : 1.0;
    double kFactor = (isQualifying ? K_QUALY : K_RACE) * seasonDensity;

    int n = (int)grid.size();

    // ============================================================
    // STEP 1 — Build blended potential scores
    // Each driver's expected strength = 50% personal Elo + 50% team Elo.
    // This separates talent from machinery when predicting finishing order.
    // ============================================================
    struct Competitor {
        Driver* d;
        double potential;
    };

    std::vector<Competitor> contenders;
    contenders.reserve(n);
    for (int i = 0; i < n; ++i) {
        double teamElo = teams[grid[i]->teamId]->seasonRating;
        double p = (grid[i]->allTimeRating * 0.5) + (teamElo * 0.5);
        contenders.push_back({grid[i], p});
    }

    // Sort a copy by potential descending — this is the model's predicted finishing order.
    std::vector<Competitor> expectedGrid = contenders;
    std::sort(expectedGrid.begin(), expectedGrid.end(), [](const Competitor& a, const Competitor& b) {
        return a.potential > b.potential;
    });

    // Build a lookup: driverId -> expected position index (0 = predicted winner)
    std::unordered_map<std::string, int> expectedPosMap;
    for (int j = 0; j < (int)expectedGrid.size(); ++j) {
        expectedPosMap[expectedGrid[j].d->id] = j;
    }

    // ============================================================
    // STEP 2 — Driver rating updates
    // For each driver: compute how much they beat or missed expectations,
    // apply tanh smoothing to prevent outlier spikes, add podium bonuses,
    // then run a head-to-head Elo duel against their teammate.
    // ============================================================

    // We'll accumulate raw driver movement here so STEP 3 can use it
    // for the constructor update without re-computing.

    std::unordered_map<std::string, double> driverMovement; // driverId -> net movement this race

    for (int i = 0; i < n; ++i) {
        Driver* currentDriver = grid[i];
        int actualPos   = i;
        int expectedPos = expectedPosMap[currentDriver->id];

        // --- Career Weight (arctan curve) ---
        // Rookies start at 0.1, asymptotically approaching 1.0 around 120 races.
        // Prevents a single fluke result from permanently inflating a new driver.
        double careerWeight = std::atan(currentDriver->raceCount / 40.0) / (M_PI / 2.2);
        if (careerWeight < 0.1) careerWeight = 0.1;

        // --- Tanh Smoothing ---
        // Compresses extreme position swings (winning from P18 due to a safety car, etc).
        // Positive rawDelta = outperformed expectations (expected lower, finished higher).
        double rawDelta      = (double)expectedPos - (double)actualPos;
        double smoothedDelta = std::tanh(rawDelta / 15.0) * 15.0;

        double movement = (smoothedDelta * (kFactor * 0.6)) * careerWeight;

        // --- Podium Bonus ---
        // Fixed bonus on top of delta movement. Rewards podiums regardless of
        // how strongly they were predicted — finishing on the podium always matters.
        if      (actualPos == 0) movement += (15.0 * careerWeight * seasonDensity);
        else if (actualPos == 1) movement += (10.0 * careerWeight * seasonDensity);
        else if (actualPos == 2) movement += (6.0  * careerWeight * seasonDensity);

        currentDriver->allTimeRating += movement;
        driverMovement[currentDriver->id] = movement;

        // --- Teammate Duel (standard head-to-head Elo) ---
        // Find this driver's first teammate in the grid and run a clean
        // head-to-head exchange. Controls for car quality — both drivers
        // have the same machinery, so relative finishing is a pure skill signal.
        for (int j = 0; j < n; ++j) {
            Driver* peer = grid[j];
            if (peer->teamId == currentDriver->teamId && peer->id != currentDriver->id) {
                double winProb = 1.0 / (1.0 + std::pow(10.0,
                                 (peer->allTimeRating - currentDriver->allTimeRating) / 400.0));
                double actualOutcome = (actualPos < j) ? 1.0 : 0.0;

                double duelShift = (kFactor * (actualOutcome - winProb)) * careerWeight;
                currentDriver->allTimeRating += duelShift;

                // Absorb duel shift into the tracked movement so it feeds the team calc
                driverMovement[currentDriver->id] += duelShift;
                break;
            }
        }

        currentDriver->raceCount++;
        currentDriver->updatePeak();
    }

    // ============================================================
    // STEP 3 — Constructor rating update (iRating-style, field-relative)
    //
    // This is the core fix. We compute each constructor's net race performance
    // by running a proper Elo exchange against every OTHER constructor in the
    // race. A team gains Elo from every rival it finished ahead of and loses
    // to every rival that finished ahead of it. The exchange scales with the
    // Elo gap between the two teams (standard Elo expected-score formula).
    //
    // "Finishing position" for a constructor is the average finishing position
    // of its two drivers — we use their actual grid index, averaged.
    // A single-driver team uses that driver's position directly.
    //
    // This means:
    //  - A midfield team finishing P7/P8 beats every team behind them
    //    and loses to every team ahead — truly neutral around the median.
    //  - Red Bull winning P1/P2 gains rating from every other team.
    //  - A team that DNFs both drivers correctly loses heavily.
    //
    // The K-factor for team exchanges is a fraction of the race K so
    // team rating moves more slowly than individual driver rating —
    // reflecting that constructors represent a whole season, not one race.
    // ============================================================

    // Aggregate finishing positions by constructor
    struct TeamRaceResult {
        double totalPos = 0.0;
        int    driverCount = 0;
    };
    std::unordered_map<std::string, TeamRaceResult> teamResults;

    for (int i = 0; i < n; ++i) {
        std::string tid = grid[i]->teamId;
        teamResults[tid].totalPos += (double)i;
        teamResults[tid].driverCount++;
    }

    // Collect unique constructors that actually raced
    std::vector<std::string> racingTeams;
    racingTeams.reserve(teamResults.size());
    for (auto& kv : teamResults) racingTeams.push_back(kv.first);

    double teamK = kFactor * 0.3; // Team moves slower than individual drivers

    // All-pairs Elo exchange between constructors
    // For each pair (A, B): A beats B if A's avg position is numerically lower (better).
    // We accumulate deltas first, then apply all at once to avoid order-of-application bias.
    std::unordered_map<std::string, double> teamDeltas;
    for (auto& tid : racingTeams) teamDeltas[tid] = 0.0;

    for (int a = 0; a < (int)racingTeams.size(); ++a) {
        for (int b = a + 1; b < (int)racingTeams.size(); ++b) {
            const std::string& tidA = racingTeams[a];
            const std::string& tidB = racingTeams[b];

            double avgPosA = teamResults[tidA].totalPos / teamResults[tidA].driverCount;
            double avgPosB = teamResults[tidB].totalPos / teamResults[tidB].driverCount;

            double ratingA = teams[tidA]->seasonRating;
            double ratingB = teams[tidB]->seasonRating;

            // Expected score for A (probability A beats B based on current Elo)
            double expectedA = 1.0 / (1.0 + std::pow(10.0, (ratingB - ratingA) / 400.0));
            // Actual outcome: A scores 1.0 if it finished ahead (lower avg pos), else 0.0
            double actualA = (avgPosA < avgPosB) ? 1.0 : 0.0;

            double delta = teamK * (actualA - expectedA);
            teamDeltas[tidA] += delta;
            teamDeltas[tidB] -= delta; // Zero-sum: what A gains, B loses
        }
    }

    // Apply accumulated deltas and update peaks
    for (auto& kv : teamDeltas) {
        Constructor* team = teams[kv.first];
        team->seasonRating += kv.second;
        team->updatePeak();
    }
}
