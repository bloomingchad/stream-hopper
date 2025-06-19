#ifndef PRELOADSTRATEGY_H
#define PRELOADSTRATEGY_H

#include "AppState.h" // For HopperMode and NavEvent
#include <unordered_set>
#include <vector>

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
};

} // namespace Strategy

#endif // PRELOADSTRATEGY_H
