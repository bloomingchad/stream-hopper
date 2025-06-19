#ifndef MPVEVENTHANDLER_H
#define MPVEVENTHANDLER_H

#include <mpv/client.h>
#include <string>
#include <vector>
#include <functional>
#include "StationManager.h" // For StationManagerMessage

// Forward declarations
class RadioStream;
class AppState;

class MpvEventHandler {
public:
    MpvEventHandler(
        std::vector<RadioStream>& stations,
        AppState& app_state,
        std::function<void(StationManagerMessage)> poster
    );

    // Main entry point for handling an event
    void handleEvent(mpv_event* event);

private:
    // Event-specific handlers
    void handlePropertyChange(mpv_event* event);

    // Property-specific handlers
    void onTitleProperty(mpv_event_property* prop, RadioStream& station);
    void onBitrateProperty(mpv_event_property* prop, RadioStream& station);
    void onEofProperty(mpv_event_property* prop, RadioStream& station);
    void onCoreIdleProperty(mpv_event_property* prop, RadioStream& station);

    // Logic handlers
    void onTitleChanged(RadioStream& station, const std::string& new_title);
    void onStreamEof(RadioStream& station);

    // Utility methods
    RadioStream* findStationById(int station_id);
    bool contains_ci(const std::string& haystack, const std::string& needle);

    // --- State & Communication ---
    // These are references to the state owned by StationManager
    std::vector<RadioStream>& m_stations;
    AppState& m_app_state;

    // A callback to post messages back to the StationManager's queue
    std::function<void(StationManagerMessage)> m_poster;
};

#endif // MPVEVENTHANDLER_H
