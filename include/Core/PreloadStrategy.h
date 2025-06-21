#ifndef PRELOADSTRATEGY_H
#define PRELOADSTRATEGY_H

#include "AppState.h" // For HopperMode and NavEvent
#include <unordered_set>
#include <vector>
#include <utility> // For std::pair
#include <deque>   // For std::deque

namespace Strategy {

// This class encapsulates the logic for deciding which stations to keep
// active (and thus pre-loaded) based on the current application state.
class Preloader {
public:
    Preloader() = default;

    // Calculates which station indices should be active (pre-loaded)
    // based on the current mode and user navigation patterns.
    std::unordered_set<int> calculate_active_indices(
        int active_idx,
        int station_count,
        HopperMode hopper_mode,
        const std::deque<NavEvent>& nav_history
    ) const;

private:
    // Determines the number of stations to preload up and down,
    // accounting for navigation acceleration.
    std::pair<int, int> getPreloadCounts(const std::deque<NavEvent>& nav_history) const;
};

} // namespace Strategy

#endif // PRELOADSTRATEGY_H
