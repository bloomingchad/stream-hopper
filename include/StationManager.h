#ifndef STATIONMANAGER_H
#define STATIONMANAGER_H

#include "RadioStream.h"
#include "Core/PreloadStrategy.h"
#include "UI/StateSnapshot.h"
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
#include "nlohmann/json.hpp"

class MpvEventHandler;

namespace Msg {
    struct NavigateUp {};
    struct NavigateDown {};
    struct ToggleMute {};
    struct ToggleAutoHop {};
    struct ToggleFavorite {};
    struct ToggleDucking {};
    struct ToggleCopyMode {};
    struct ToggleHopperMode {};
    struct SwitchPanel {};
    struct UpdateAndPoll {}; 
    struct Quit {};
}

using StationManagerMessage = std::variant<
    Msg::NavigateUp, Msg::NavigateDown, Msg::ToggleMute, Msg::ToggleAutoHop,
    Msg::ToggleFavorite, Msg::ToggleDucking, Msg::ToggleCopyMode,
    Msg::ToggleHopperMode, Msg::SwitchPanel, Msg::UpdateAndPoll, Msg::Quit
>;

class StationManager {
public:
    StationManager(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data);
    ~StationManager();

    void post(StationManagerMessage message);
    StateSnapshot createSnapshot() const;

    // The "backchannel" flag for the UI thread
    std::atomic<bool>& getNeedsRedrawFlag();
    std::atomic<bool>& getQuitFlag();

private:
    friend class MpvEventHandler;

    // ... (rest of the class definition is identical)
    struct ActiveFade {
        int station_id;
        int generation;
        double start_vol;
        double target_vol;
        std::chrono::steady_clock::time_point start_time;
        int duration_ms;
    };

    void actorLoop();
    void handle_activeFades();
    void pollMpvEvents();
    void handle_navigate(NavDirection direction);
    void handle_toggleMute();
    void handle_toggleAutoHop();
    void handle_toggleFavorite();
    void handle_toggleDucking();
    void handle_toggleCopyMode();
    void handle_toggleHopperMode();
    void handle_switchPanel();
    void handle_updateAndPoll();
    void handle_quit();
    void updateActiveWindow();
    void fadeAudio(int station_id, double to_vol, int duration_ms);
    void initializeStation(int station_idx);
    void shutdownStation(int station_idx);
    void saveHistoryToDisk();
    void addHistoryEntry(const std::string& station_name, const nlohmann::json& entry);

    mutable std::mutex m_stations_mutex;
    std::vector<RadioStream> m_stations;
    std::vector<ActiveFade> m_active_fades;
    std::unordered_set<int> m_active_station_indices;
    Strategy::Preloader m_preloader;
    std::unique_ptr<MpvEventHandler> m_event_handler;
    std::unique_ptr<nlohmann::json> m_song_history;
    int m_unsaved_history_count;
    int m_active_station_idx;
    bool m_copy_mode_active;
    ActivePanel m_active_panel;
    int m_history_scroll_offset;
    std::chrono::steady_clock::time_point m_copy_mode_start_time;
    bool m_auto_hop_mode_active;
    std::chrono::steady_clock::time_point m_auto_hop_start_time;
    HopperMode m_hopper_mode;
    std::chrono::steady_clock::time_point m_last_switch_time;
    std::deque<NavEvent> m_nav_history;
    std::chrono::steady_clock::time_point m_session_start_time;
    int m_session_switches;
    int m_new_songs_found;
    int m_songs_copied;
    bool m_was_quit_by_mute_timeout;

    // Main loop control flags
    std::atomic<bool> m_quit_flag;
    std::atomic<bool> m_needs_redraw; // FIX: Add the flag back

    std::thread m_actor_thread;
    std::deque<StationManagerMessage> m_message_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cond;
};

#endif // STATIONMANAGER_H
