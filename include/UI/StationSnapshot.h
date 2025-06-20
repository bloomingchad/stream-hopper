#ifndef STATIONSNAPSHOT_H
#define STATIONSNAPSHOT_H

#include <string>
#include <vector>
#include "RadioStream.h" // For PlaybackState enum

// A plain-old-data struct containing only what the UI needs to render a station.
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

// A struct to hold a guaranteed-consistent snapshot of the data needed for the UI.
// This is created and returned by the StationManager to avoid race conditions.
struct StationSnapshot {
    std::vector<StationDisplayData> stations;
    int active_station_idx;
};

#endif // STATIONSNAPSHOT_H
