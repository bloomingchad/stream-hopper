#ifndef MPVEVENTHANDLER_H
#define MPVEVENTHANDLER_H

#include <mpv/client.h>
#include <string>
#include <vector>
#include <functional>

// Forward declarations
class RadioStream;
class AppState;
class StationManager; // Only needed for StationManagerMessage, but variant is complex
#include "StationManager.h" // Keep full include for StationManagerMessage variant

class MpvEventHandler {
public:
    MpvEventHandler(
        std::vector<RadioStream>& stations,
        AppState& app_state,
        std::function<void(StationManagerMessage)> poster
    );

    void handleEvent(mpv_event* event);

private:
    void handlePropertyChange(mpv_event* event);
    void onTitleProperty(mpv_event_property* prop, RadioStream& station);
    void onBitrateProperty(mpv_event_property* prop, RadioStream& station);
    void onEofProperty(mpv_event_property* prop, RadioStream& station);
    void onCoreIdleProperty(mpv_event_property* prop, RadioStream& station);
    void onTitleChanged(RadioStream& station, const std::string& new_title);
    void onStreamEof(RadioStream& station);

    RadioStream* findStationById(int station_id);

    std::vector<RadioStream>& m_stations;
    AppState& m_app_state;
    std::function<void(StationManagerMessage)> m_poster;
};

#endif // MPVEVENTHANDLER_H
