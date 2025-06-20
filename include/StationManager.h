#ifndef STATIONMANAGER_H
#define STATIONMANAGER_H

#include "RadioStream.h"
#include "Core/PreloadStrategy.h"
#include "AppState.h"
#include "UI/StationDisplayData.h"
#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <variant>
#include <unordered_set>

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

    void post(StationManagerMessage message);
    std::vector<StationDisplayData> getStationDisplayData() const;

private:
    // A struct to hold the state of an active fade animation
    struct ActiveFade {
        int station_id;
        int generation;
        double start_vol;
        double target_vol;
        std::chrono::steady_clock::time_point start_time;
        int duration_ms;
    };

    void actorLoop();
    void processMessages();
    void handle_activeFades();
    void pollMpvEvents();
    
    void handle_switchStation(int old_idx, int new_idx);
    void handle_toggleMute(int station_idx);
    void handle_toggleDucking(int station_idx);
    void handle_toggleFavorite(int station_idx);
    void handle_setHopperMode(HopperMode new_mode);
    void handle_updateActiveWindow();
    void handle_saveHistory();
    void handle_shutdown();

    void fadeAudio(int station_id, double to_vol, int duration_ms);
    void initializeStation(int station_idx);
    void shutdownStation(int station_idx);

    mutable std::mutex m_stations_mutex;
    std::vector<RadioStream> m_stations;
    std::vector<ActiveFade> m_active_fades; // New state for fades

    AppState& m_app_state;
    Strategy::Preloader m_preloader;
    std::unique_ptr<MpvEventHandler> m_event_handler;
    std::unordered_set<int> m_active_station_indices;

    std::thread m_actor_thread;
    std::deque<StationManagerMessage> m_message_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cond;
};

#endif // STATIONMANAGER_H
