#ifndef STATIONMANAGER_H
#define STATIONMANAGER_H

#include "RadioStream.h"
#include "Core/PreloadStrategy.h"
#include "Core/MessageHandler.h" // Contains message definitions now
#include "Core/UpdateManager.h" 
#include "SessionState.h"
#include "UI/StateSnapshot.h"
#include "PersistenceManager.h"
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

// Message definitions are now in MessageHandler.h

/*!

@class StationManager

@brief The central actor managing all application state and audio playback.

This class runs in its own dedicated thread and processes messages from a queue

to ensure all state modifications and interactions with the libmpv C-API are

thread-safe. It is the single source of truth for application state.

--- UI Redraw Philosophy (IMPORTANT) ---

The m_needs_redraw flag is the sole mechanism for triggering a UI update.

It must be set to true whenever the internal state changes in a way that

needs to be visually communicated to the user. Failure to set the flag results

in a stale, unresponsive UI. Setting it unnecessarily causes excessive CPU usage

and screen flicker.

A redraw is warranted under the following conditions:

Direct User Actions (Immediate Feedback):

A redraw is required when the user performs an action and expects to see an

instant result. This includes: Navigation (moving the highlight, updating

panels for the new context), Toggles (updating icons and volume bars for Mute,

Favorite, or Ducking), Panel Focus (coloring the active panel's border), URL

Cycling (showing in-progress and success/failure feedback), and Mode Switching

(updating headers/footers to reflect the new global mode).

Asynchronous Backend Events (Live Data Updates):

A redraw is required when the application receives new information from a

background source like mpv. This includes: a New Song Title (updating the

"Now Playing" and "History" panels is a core feature), a change in Stream

Quality (updating the bitrate indicator), a Stream Buffering event (showing a

"Buffering..." indicator), or a Stream Error (showing a "Reconnecting..."

message).

Internal Timers and State Transitions (Automated Updates):

A redraw is required when the application's internal logic triggers a visual

change. This includes: each step of an Audio Fade to animate the volume bar,

ticks of the Auto-Hop Timer to update the progress bar, and the clearing of

temporary status indicators (like a success/fail icon) after their timeout.

Adherence to these principles is critical to prevent UI regressions.
*/
class StationManager {
public:
    StationManager(const StationData& station_data);
    ~StationManager();
    void post(StationManagerMessage message);
    StateSnapshot createSnapshot() const;
    std::atomic<bool>& getNeedsRedrawFlag();
    std::atomic<bool>& getQuitFlag();

private:
    friend class MpvEventHandler;
    friend class MessageHandler;
    friend class UpdateManager;

    // Methods that are NOT message handlers or update handlers remain here
    void actorLoop();
    void pollMpvEvents();
    void crossFadeToPending(int station_id);
    void updateActiveWindow();
    void fadeAudio(int station_id, double to_vol, int duration_ms, bool for_pending = false);
    void initializeStation(int station_idx);
    void shutdownStation(int station_idx);
    void saveHistoryToDisk();
    void addHistoryEntry(const std::string& station_name, const nlohmann::json& entry);

    struct ActiveFade {
        int station_id; int generation; double start_vol; double target_vol;
        std::chrono::steady_clock::time_point start_time; int duration_ms;
        bool is_for_pending_instance; 
    };

    // Core Components & Data
    mutable std::mutex m_stations_mutex;
    std::vector<RadioStream> m_stations;
    std::vector<ActiveFade> m_active_fades;
    std::unordered_set<int> m_active_station_indices;
    Strategy::Preloader m_preloader;
    std::unique_ptr<MpvEventHandler> m_event_handler;
    std::unique_ptr<MessageHandler> m_message_handler;
    std::unique_ptr<UpdateManager> m_update_manager;
    std::unique_ptr<nlohmann::json> m_song_history;
    int m_unsaved_history_count;

    // Encapsulated Application State
    SessionState m_session_state;

    // Actor Model Internals
    std::atomic<bool> m_quit_flag;
    std::atomic<bool> m_needs_redraw;
    std::thread m_actor_thread;
    std::deque<StationManagerMessage> m_message_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cond;

    // Constants
    static constexpr size_t MAX_NAV_HISTORY = 10;
    static constexpr int HISTORY_WRITE_THRESHOLD = 5;
};

#endif // STATIONMANAGER_H
