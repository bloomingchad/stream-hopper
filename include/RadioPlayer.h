// include/RadioPlayer.h
#ifndef RADIOPLAYER_H
#define RADIOPLAYER_H

#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include "RadioStream.h"
#include "nlohmann/json_fwd.hpp" // Use forward declaration

// Forward declare UIManager to avoid circular dependency
class UIManager;

class RadioPlayer {
public:
    RadioPlayer(std::vector<std::pair<std::string, std::string>> station_data);
    ~RadioPlayer();
    void run();

private:
    // Member Variables
    std::unique_ptr<UIManager> m_ui;
    std::vector<RadioStream> m_stations;
    std::atomic<int> m_active_station_idx;
    std::atomic<bool> m_quit_flag;
    std::atomic<bool> m_small_mode_active;
    
    // History
    std::unique_ptr<nlohmann::json> m_song_history;
    std::mutex m_history_mutex;
    
    // MPV Event Handling
    std::thread m_mpv_event_thread;

    // Small Mode Timing
    std::chrono::steady_clock::time_point m_small_mode_start_time;
    int m_station_switch_duration;

    // Methods
    void initialize_stations();
    void mpv_event_loop();
    void handle_mpv_event(mpv_event* event);
    void handle_input(int ch);
    bool update_state();

    // Key Actions
    void on_key_up();
    void on_key_down();
    void on_key_enter();
    void toggle_small_mode();

    // Station & Audio Control
    void switch_station(int new_idx);
    void toggle_mute_station(int station_idx);
    void fade_audio(RadioStream& station, double from_vol, double to_vol, int duration_ms);

    // History Management
    void on_title_changed(RadioStream& station, const std::string& new_title);
    void on_stream_eof(RadioStream& station);
    void load_history_from_disk();
    void save_history_to_disk();

    // Helpers
    RadioStream* find_station_by_id(int station_id);
    bool should_switch_station();
    int get_remaining_seconds_for_current_station();
};

#endif // RADIOPLAYER_H
