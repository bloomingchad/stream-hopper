// RadioPlayer.h
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include "json.hpp" // Reverted to nlohmann/json
#include "RadioStream.h"

// Forward declaration to reduce header dependencies
class UIManager;

class RadioPlayer {
public:
    RadioPlayer(std::vector<std::pair<std::string, std::string>> station_data);
    ~RadioPlayer();

    void run();

private:
    // Member Functions
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
    nlohmann::json m_song_history; // Reverted to nlohmann::json
    std::mutex m_history_mutex;
};
