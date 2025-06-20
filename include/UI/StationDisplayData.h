#ifndef STATIONDISPLAYDATA_H
#define STATIONDISPLAYDATA_H

#include <string>
#include "RadioStream.h" // For PlaybackState enum

// A plain-old-data struct containing only what the UI needs to render a station.
// This is created by the StationManager and passed BY VALUE to the UI thread
// to prevent data races.
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

#endif // STATIONDISPLAYDATA_H
