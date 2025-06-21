#ifndef STATESNAPSHOT_H
#define STATESNAPSHOT_H

#include <string>
#include <vector>
#include <chrono>
#include "RadioStream.h" 
#include "AppState.h"    
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
    int pending_bitrate; // For displaying during URL cycle
    size_t url_count;
};

// A struct to hold a guaranteed-consistent snapshot of ALL data needed for the UI.
struct StateSnapshot {
    std::vector<StationDisplayData> stations;
    int active_station_idx;
    ActivePanel active_panel;
    bool is_copy_mode_active;
    bool is_auto_hop_mode_active;
    int history_scroll_offset;
    HopperMode hopper_mode;
    double current_volume_for_header;
    nlohmann::json active_station_history;
    int auto_hop_remaining_seconds;
    int auto_hop_total_duration;
};

#endif // STATESNAPSHOT_H
