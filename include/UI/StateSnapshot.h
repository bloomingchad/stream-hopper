#ifndef STATESNAPSHOT_H
#define STATESNAPSHOT_H

#include <chrono>
#include <string>
#include <vector>

#include "AppState.h"
#include "RadioStream.h"
#include "nlohmann/json.hpp"

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
    CyclingState cycling_state;
    std::string pending_title;
    int pending_bitrate; // For displaying during URL cycle
    size_t url_count;
    double volume_offset; // For normalization UI
};

// A struct to hold a guaranteed-consistent snapshot of ALL data needed for the UI.
struct StateSnapshot {
    std::vector<StationDisplayData> stations;
    int active_station_idx;
    ActivePanel active_panel;
    AppMode app_mode;
    bool is_copy_mode_active;
    bool is_auto_hop_mode_active;
    int history_scroll_offset;
    HopperMode hopper_mode;
    double current_volume_for_header;
    nlohmann::json active_station_history;
    int auto_hop_remaining_seconds;
    int auto_hop_total_duration;
    std::string temporary_status_message;      // New field for UI feedback
    bool is_volume_offset_mode_active = false; // Is the offset slider visible?
    bool is_fetching_stations = false;         // Is a fetch for random stations in progress?
};

#endif // STATESNAPSHOT_H
