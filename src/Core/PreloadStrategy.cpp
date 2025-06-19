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

std::unordered_set<int> Preloader::calculate_active_indices(
    int active_idx,
    int station_count,
    HopperMode hopper_mode,
    const std::deque<NavEvent>& nav_history
) const {
    std::unordered_set<int> new_active_set;

    if (station_count == 0) {
        return new_active_set;
    }

    switch (hopper_mode) {
        case HopperMode::PERFORMANCE:
            for (int i = 0; i < station_count; ++i) {
                new_active_set.insert(i);
            }
            return new_active_set;

        case HopperMode::FOCUS:
            new_active_set.insert(active_idx);
            return new_active_set;

        case HopperMode::BALANCED: {
            int preload_up = PRELOAD_DEFAULT;
            int preload_down = PRELOAD_DEFAULT;

            // --- Navigation Acceleration Logic ---
            if (!nav_history.empty()) {
                const auto& last_event = nav_history.back();
                NavDirection current_dir = last_event.direction;
                int consecutive_count = 0;
                auto now = std::chrono::steady_clock::now();

                for (auto it = nav_history.rbegin(); it != nav_history.rend(); ++it) {
                    if (now - it->timestamp > ACCEL_TIME_WINDOW) break;
                    if (it->direction == current_dir) {
                        consecutive_count++;
                    } else {
                        break;
                    }
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
            }
            // --- End Acceleration Logic ---

            new_active_set.insert(active_idx);
            for (int i = 1; i <= preload_up; ++i) {
                new_active_set.insert((active_idx - i + station_count) % station_count);
            }
            for (int i = 1; i <= preload_down; ++i) {
                new_active_set.insert((active_idx + i) % station_count);
            }
            return new_active_set;
        }
    }
    // Should not be reached, but defensive return
    return new_active_set;
}

} // namespace Strategy
