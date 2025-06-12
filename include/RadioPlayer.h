// RadioPlayer.h
#pragma once

#include "nlohmann/json_fwd.hpp" // Use forward header
#include "RadioStream.h"
#include <atomic>
#include <chrono>
#include <memory> // For std::unique_ptr
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class UIManager;

class RadioPlayer {
public:
    RadioPlayer(std::vector<std::pair<std::string, std::string>> station_data);
    ~RadioPlayer();
    void run();

private:
    // ... all member functions are the same ...
    bool update_state();
    void on_key_up();
    void on_key_down();
    void on_key_enter();
    void handle_input(int ch);
    void toggle_small_mode();
    bool should_switch_station();
    int get_remaining_seconds_for_current_station();
    void switch_station(int new_idx);
    void toggle_mute_station(int station_idx);
    void fade_audio(RadioStream& station, double from_vol, double to_vol, int duration_ms);
    void mpv_event_loop();
    RadioStream* find_station_by_id(int station_id);
    void on_title_changed(RadioStream& station, const std::string& new_title);
    void on_stream_eof(RadioStream& station);
    void handle_mpv_event(struct mpv_event* event);
    void load_history_from_disk();
    void save_history_to_disk();

    // Member Variables
    std::vector<RadioStream> m_stations;
    std::unique_ptr<UIManager> m_ui;
    int m_active_station_idx;
    std::atomic<bool> m_quit_flag;
    std::thread m_mpv_event_thread;
    std::atomic<bool> m_small_mode_active;
    std::chrono::steady_clock::time_point m_small_mode_start_time;
    int m_station_switch_duration;
    // THE FIX: Use a unique_ptr to an incomplete type.
    std::unique_ptr<nlohmann::json> m_song_history;
    std::mutex m_history_mutex;
};
