#include "Core/PreloadStrategy.h"

#include <algorithm> // For std::max

namespace {
    // --- Constants moved from StationManager ---
    // The window of time to check for rapid navigation.
    constexpr auto ACCEL_TIME_WINDOW = std::chrono::milliseconds(500);
    // The number of consecutive navigation events to trigger acceleration.
    constexpr int ACCEL_EVENT_THRESHOLD = 3;
    // Default number of stations to preload in either direction.
    constexpr int PRELOAD_DEFAULT = 3;
    // How many extra stations to preload when accelerating.
    constexpr int PRELOAD_EXTRA = 3;
    // How many stations to reduce from the non-accelerating direction.
    constexpr int PRELOAD_REDUCTION = 2;
}

namespace Strategy {

    // NEW HELPER: Encapsulates the navigation acceleration logic.
    std::pair<int, int> Preloader::getPreloadCounts(const std::deque<NavEvent>& nav_history) const {
        int preload_up = PRELOAD_DEFAULT;
        int preload_down = PRELOAD_DEFAULT;

        if (nav_history.empty()) {
            return {preload_up, preload_down};
        }

        const auto& last_event = nav_history.back();
        NavDirection current_dir = last_event.direction;
        int consecutive_count = 0;
        auto now = std::chrono::steady_clock::now();

        for (auto it = nav_history.rbegin(); it != nav_history.rend(); ++it) {
            if (now - it->timestamp > ACCEL_TIME_WINDOW)
                break;
            if (it->direction != current_dir)
                break;
            consecutive_count++;
        }

        if (consecutive_count >= ACCEL_EVENT_THRESHOLD) {
            if (current_dir == NavDirection::DOWN) {
                preload_down += PRELOAD_EXTRA;
                preload_up = std::max(1, PRELOAD_DEFAULT - PRELOAD_REDUCTION);
            } else { // NavDirection::UP
                preload_up += PRELOAD_EXTRA;
                preload_down = std::max(1, PRELOAD_DEFAULT - PRELOAD_REDUCTION);
            }
        }

        return {preload_up, preload_down};
    }

    std::unordered_set<int> Preloader::calculate_active_indices(int active_idx,
                                                                int station_count,
                                                                HopperMode hopper_mode,
                                                                const std::deque<NavEvent>& nav_history) const {
        std::unordered_set<int> new_active_set;

        if (station_count == 0) {
            return new_active_set;
        }

        // Always include the currently active station.
        new_active_set.insert(active_idx);

        switch (hopper_mode) {
        case HopperMode::PERFORMANCE:
            for (int i = 0; i < station_count; ++i) {
                new_active_set.insert(i);
            }
            break;

        case HopperMode::FOCUS:
            // Only the active station is needed. Already added.
            break;

        case HopperMode::BALANCED: {
            // Delegate the complex part to the helper function.
            auto [preload_up, preload_down] = getPreloadCounts(nav_history);

            for (int i = 1; i <= preload_up; ++i) {
                new_active_set.insert((active_idx - i + station_count) % station_count);
            }
            for (int i = 1; i <= preload_down; ++i) {
                new_active_set.insert((active_idx + i) % station_count);
            }
            break;
        }
        }
        // Should not be reached, but defensive return

        return new_active_set;
    }

} // namespace Strategy
