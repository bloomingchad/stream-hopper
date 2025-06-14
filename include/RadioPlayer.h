// include/RadioPlayer.h
#ifndef RADIOPLAYER_H
#define RADIOPLAYER_H

#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>

#include "RadioStream.h"
#include "nlohmann/json.hpp"

// Forward declaration
class UIManager;

enum class ActivePanel {
    STATIONS, HISTORY
};

class RadioPlayer {
public:
    RadioPlayer(std::vector<std::pair<std::string, std::string>> station_data);
    ~RadioPlayer();

    void run();

private:
    void handle_input(int ch);
    void mpv_event_loop();
    void handle_mpv_event(struct mpv_event* event);
    
    void on_key_enter();
    void on_title_changed(RadioStream& station, const std::string& new_title);
    void on_stream_eof(RadioStream& station);

    void switch_station(int new_idx);
    void toggle_mute_station(int station_idx);
    void fade_audio(RadioStream& station, double from_vol, double to_vol, int duration_ms);

    void load_history_from_disk();
    void save_history_to_disk();
    void load_favorites_from_disk();
    void save_favorites_to_disk();
    
    void toggle_small_mode();
    bool should_switch_station();
    bool update_state();
    int get_remaining_seconds_for_current_station();

    RadioStream* find_station_by_id(int station_id);

    std::unique_ptr<UIManager> m_ui;
    std::vector<RadioStream> m_stations;
    int m_active_station_idx;
    std::atomic<bool> m_quit_flag;
    std::atomic<bool> m_needs_redraw;

    std::thread m_mpv_event_thread;

    bool m_small_mode_active;
    ActivePanel m_active_panel;
    int m_history_scroll_offset;
    
    std::unique_ptr<nlohmann::json> m_song_history;
    std::mutex m_history_mutex;

    std::chrono::steady_clock::time_point m_small_mode_start_time;
    int m_station_switch_duration;

    // --- ADDED FOR COPY MODE ---
    std::atomic<bool> m_copy_mode_active;
    std::chrono::steady_clock::time_point m_copy_mode_start_time;
};

#endif // RADIOPLAYER_H
