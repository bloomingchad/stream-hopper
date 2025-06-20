#ifndef MPVEVENTHANDLER_H
#define MPVEVENTHANDLER_H

#include <mpv/client.h>
#include <string>
#include <vector>
#include <functional>

// Forward declarations
class RadioStream;
class StationManager; // The new owner of everything

class MpvEventHandler {
public:
    // The handler now only needs a reference to its owner, the StationManager
    explicit MpvEventHandler(StationManager& manager);

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

    // A reference to the single source of truth.
    StationManager& m_manager;
};

#endif // MPVEVENTHANDLER_H
