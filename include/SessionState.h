#ifndef SESSIONSTATE_H
#define SESSIONSTATE_H

#include "AppState.h"
#include <chrono>
#include <deque>

/**
 * @class SessionState
 * @brief A data class that consolidates all non-station-specific state.
 */
struct SessionState {
    // Core State
    int active_station_idx = 0;
    HopperMode hopper_mode = HopperMode::BALANCED;

    // UI State
    ActivePanel active_panel = ActivePanel::STATIONS;
    int history_scroll_offset = 0;

    // Mode States & Timers
    bool copy_mode_active = false;
    std::chrono::steady_clock::time_point copy_mode_start_time;

    bool auto_hop_mode_active = false;
    std::chrono::steady_clock::time_point auto_hop_start_time;

    // Navigation & Preloading State
    std::deque<NavEvent> nav_history;
    std::chrono::steady_clock::time_point last_switch_time;

    // Session Statistics & Lifecycle
    std::chrono::steady_clock::time_point session_start_time;
    int session_switches = 0;
    int new_songs_found = 0;
    int songs_copied = 0;
    bool was_quit_by_mute_timeout = false;

    SessionState() : last_switch_time(std::chrono::steady_clock::now()),
                     session_start_time(std::chrono::steady_clock::now()) {}
};

#endif // SESSIONSTATE_H
