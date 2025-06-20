#ifndef STATESNAPSHOT_H
#define STATESNAPSHOT_H

#include <string>
#include <vector>
#include <chrono>
#include "RadioStream.h" // For PlaybackState enum
#include "AppState.h"    // For HopperMode, ActivePanel enums
#include "nlohmann/json.hpp" // <-- CRITICAL FIX: Include the JSON library header

// A plain-old-data struct containing only what the UI needs to render a single station.
struct StationDisplayData {
    std::string name;
    std::string current_title;
    int bitrate;
    double current_volume;
    bool is_initialized;
    bool is_favorite;
    bool is_buffering;
    PlaybackState playback_state;
};

// A struct to hold a guaranteed-consistent snapshot of ALL data needed for the UI.
// This is created and returned by the StationManager to enforce unidirectional data flow.
struct StateSnapshot {
    // Station Data
    std::vector<StationDisplayData> stations;
    int active_station_idx;

    // UI State
    ActivePanel active_panel;
    bool is_copy_mode_active;
    bool is_auto_hop_mode_active;
    int history_scroll_offset;
    HopperMode hopper_mode;

    // Data for UI rendering
    double current_volume_for_header; // Volume of active station, or 0 if muted/uninit
    nlohmann::json active_station_history;

    // Auto-Hop Timer Data
    int auto_hop_remaining_seconds;
    int auto_hop_total_duration;
};

#endif // STATESNAPSHOT_H
