#ifndef STATIONMANAGER_H
#define STATIONMANAGER_H

#include "RadioStream.h"
#include "Core/PreloadStrategy.h"
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

// Forward declare the new handler class
class MpvEventHandler;

namespace Msg {
    struct SwitchStation { int old_idx; int new_idx; };
    struct ToggleMute { int station_idx; };
    struct ToggleDucking { int station_idx; };
    struct ToggleFavorite { int station_idx; };
    struct SetHopperMode { HopperMode new_mode; };
    struct UpdateActiveWindow {};
    struct SaveHistory {};
    struct Shutdown {};
}

using StationManagerMessage = std::variant<
    Msg::SwitchStation,
    Msg::ToggleMute,
    Msg::ToggleDucking,
    Msg::ToggleFavorite,
    Msg::SetHopperMode,
    Msg::UpdateActiveWindow,
    Msg::SaveHistory,
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
    bool processNextMessage(); // NEW: Helper to process one message from the queue.
    void pollMpvEvents();

    // --- Message Handlers ---
    void handle_switchStation(int old_idx, int new_idx);
    void handle_toggleMute(int station_idx);
    void handle_toggleDucking(int station_idx);
    void handle_toggleFavorite(int station_idx);
    void handle_setHopperMode(HopperMode new_mode);
    void handle_updateActiveWindow();
    void handle_saveHistory();
    void handle_shutdown();

    // --- Other Helpers ---
    void fadeAudio(RadioStream& station, double from_vol, double to_vol, int duration_ms);
    bool isFadeStillValid(const RadioStream& station, int captured_generation, double target_volume) const;
    void cleanupFinishedFutures();
    void initializeStation(int station_idx);
    void shutdownStation(int station_idx);

    // --- State and Sub-components ---
    std::vector<RadioStream> m_stations;
    AppState& m_app_state;
    Strategy::Preloader m_preloader;
    std::unique_ptr<MpvEventHandler> m_event_handler; // NEW: The handler is now a component
    std::unordered_set<int> m_active_station_indices;

    // --- Actor components ---
    std::thread m_actor_thread;
    std::deque<StationManagerMessage> m_message_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cond;

    // --- Other async management ---
    std::vector<std::future<void>> m_fade_futures;
    std::mutex m_fade_futures_mutex;
};

#endif // STATIONMANAGER_H
