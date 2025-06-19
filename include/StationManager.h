#ifndef STATIONMANAGER_H
#define STATIONMANAGER_H

#include "RadioStream.h"
#include "Core/PreloadStrategy.h" // NEW INCLUDE
#include "AppState.h"
#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <future>
#include <variant>
#include <unordered_set>

//
// --- The Message Definitions (Actor Model) ---
// This is now defined *outside* and *before* the StationManager class.
//
namespace Msg {
    struct SwitchStation { int old_idx; int new_idx; };
    struct ToggleMute { int station_idx; };
    struct ToggleDucking { int station_idx; };
    struct ToggleFavorite { int station_idx; };
    struct SetHopperMode { HopperMode new_mode; };
    struct UpdateActiveWindow {};
    struct SaveHistory {}; // NEW: Message to trigger a history save
    struct Shutdown {};
}

// A type-safe container for any possible message
using StationManagerMessage = std::variant<
    Msg::SwitchStation,
    Msg::ToggleMute,
    Msg::ToggleDucking,
    Msg::ToggleFavorite,
    Msg::SetHopperMode,
    Msg::UpdateActiveWindow,
    Msg::SaveHistory, // NEW
    Msg::Shutdown
>;


class StationManager {
public:
    StationManager(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data, AppState& app_state);
    ~StationManager();

    // The ONLY public method to interact with the manager. It's thread-safe.
    void post(StationManagerMessage message);

    const std::vector<RadioStream>& getStations() const;

private:
    // --- The Actor's own thread and event loop ---
    void actorLoop();
    void pollMpvEvents();

    // --- Private message handlers. These are NOT thread-safe and are only ever called by the actorLoop ---
    void handle_switchStation(int old_idx, int new_idx);
    void handle_toggleMute(int station_idx);
    void handle_toggleDucking(int station_idx);
    void handle_toggleFavorite(int station_idx);
    void handle_setHopperMode(HopperMode new_mode);
    void handle_updateActiveWindow();
    void handle_saveHistory(); // NEW: Handler for the message

    // --- Private MPV event handlers (called by the actor thread) ---
    void handleMpvEvent(mpv_event* event);
    void handlePropertyChange(mpv_event* event);
    void onTitleProperty(mpv_event_property* prop, RadioStream& station);
    void onBitrateProperty(mpv_event_property* prop, RadioStream& station);
    void onEofProperty(mpv_event_property* prop, RadioStream& station);
    void onCoreIdleProperty(mpv_event_property* prop, RadioStream& station);
    void onTitleChanged(RadioStream& station, const std::string& new_title);
    void onStreamEof(RadioStream& station);
    
    // --- Other private helpers (called by the actor thread) ---
    void fadeAudio(RadioStream& station, double from_vol, double to_vol, int duration_ms);
    RadioStream* findStationById(int station_id);
    bool contains_ci(const std::string& haystack, const std::string& needle);
    void cleanupFinishedFutures();
    void initializeStation(int station_idx);
    void shutdownStation(int station_idx);

    // --- State (only accessed by the actor thread) ---
    std::vector<RadioStream> m_stations;
    AppState& m_app_state;
    Strategy::Preloader m_preloader; // NEW MEMBER
    std::unordered_set<int> m_active_station_indices;

    // --- Actor components ---
    std::thread m_actor_thread;
    std::deque<StationManagerMessage> m_message_queue;
    std::mutex m_queue_mutex; // The ONLY mutex needed for major state changes.
    std::condition_variable m_queue_cond;

    // --- Other async management ---
    std::vector<std::future<void>> m_fade_futures;
    std::mutex m_fade_futures_mutex; // Kept separate for fine-grained locking on futures.
};

#endif // STATIONMANAGER_H
