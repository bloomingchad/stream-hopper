// src/RadioPlayer.cpp
#include "RadioPlayer.h"
#include "UIManager.h"
#include "nlohmann/json.hpp" // Include the full JSON header for implementation
#include "Utils.h" // Include our utility header

#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ncurses.h>

// Define configuration constants
#define FADE_TIME_MS 900
#define SMALL_MODE_TOTAL_TIME_SECONDS 720

using nlohmann::json;

RadioPlayer::RadioPlayer(std::vector<std::pair<std::string, std::string>> station_data)
    : m_ui(std::make_unique<UIManager>()),
      m_active_station_idx(0),
      m_quit_flag(false),
      m_small_mode_active(false),
      m_small_mode_start_time(std::chrono::steady_clock::now()),
      m_station_switch_duration(0),
      m_song_history(std::make_unique<json>(json::object())) // Initialize the unique_ptr
{
    if (station_data.empty()) {
        throw std::runtime_error("No radio stations provided.");
    }
    for (size_t i = 0; i < station_data.size(); ++i) {
        m_stations.emplace_back(i, station_data[i].first, station_data[i].second);
    }
    if (!m_stations.empty()) {
        m_station_switch_duration = SMALL_MODE_TOTAL_TIME_SECONDS / static_cast<int>(m_stations.size());
    }
    load_history_from_disk();
    for (int i = 0; i < static_cast<int>(m_stations.size()); ++i) {
        double initial_volume = (i == m_active_station_idx) ? 100.0 : 0.0;
        m_stations[i].initialize(initial_volume);
    }
}

RadioPlayer::~RadioPlayer() {
    m_quit_flag = true;
    if (m_mpv_event_thread.joinable()) {
        m_mpv_event_thread.join();
    }
    save_history_to_disk();
}

void RadioPlayer::run() {
    m_mpv_event_thread = std::thread(&RadioPlayer::mpv_event_loop, this);
    bool needs_redraw = true;

    while (!m_quit_flag) {
        if (needs_redraw) {
            // THE FIX IS HERE: Pass the history object to the draw function.
            m_ui->draw(m_stations, m_active_station_idx, m_small_mode_active, *m_song_history);
            needs_redraw = false;
        }
        
        if (update_state()) {
            needs_redraw = true;
        }
        
        int ch = m_ui->getInput();
        if (ch != ERR) {
            handle_input(ch);
            needs_redraw = true;
        }
    }
}

bool RadioPlayer::update_state() {
    if (m_small_mode_active && should_switch_station()) {
        int next_station = (m_active_station_idx + 1) % static_cast<int>(m_stations.size());
        switch_station(next_station);
        m_small_mode_start_time = std::chrono::steady_clock::now();
        return true;
    }
    for (const auto& station : m_stations) {
        if (station.isFading()) {
            return true;
        }
    }
    if (m_small_mode_active) {
        return true;
    }
    return false;
}

void RadioPlayer::on_key_up() {
    if (!m_small_mode_active && m_active_station_idx > 0) {
        switch_station(m_active_station_idx - 1);
    }
}

void RadioPlayer::on_key_down() {
    if (!m_small_mode_active && m_active_station_idx < static_cast<int>(m_stations.size()) - 1) {
        switch_station(m_active_station_idx + 1);
    }
}

void RadioPlayer::on_key_enter() {
    if (!m_small_mode_active) {
        toggle_mute_station(m_active_station_idx);
    }
}

void RadioPlayer::handle_input(int ch) {
    switch (ch) {
        case KEY_UP:    on_key_up(); break;
        case KEY_DOWN:  on_key_down(); break;
        case '\n': case '\r': case KEY_ENTER: on_key_enter(); break;
        case 's': case 'S': toggle_small_mode(); break;
        case 'q': case 'Q': m_quit_flag = true; break;
    }
}

void RadioPlayer::toggle_small_mode() {
    m_small_mode_active = !m_small_mode_active;
    if (m_small_mode_active) {
        m_small_mode_start_time = std::chrono::steady_clock::now();
        RadioStream& current_station = m_stations[m_active_station_idx];
        if (current_station.isMuted()) {
            toggle_mute_station(m_active_station_idx);
        } else if (current_station.getCurrentVolume() < 50.0) {
            fade_audio(current_station, current_station.getCurrentVolume(), 100.0, FADE_TIME_MS);
        }
    }
}

bool RadioPlayer::should_switch_station() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_small_mode_start_time);
    return elapsed.count() >= m_station_switch_duration;
}

int RadioPlayer::get_remaining_seconds_for_current_station() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_small_mode_start_time);
    return std::max(0, m_station_switch_duration - static_cast<int>(elapsed.count()));
}

void RadioPlayer::switch_station(int new_idx) {
    if (new_idx == m_active_station_idx) return;
    RadioStream& current_station = m_stations[m_active_station_idx];
    if (!current_station.isMuted()) {
        fade_audio(current_station, current_station.getCurrentVolume(), 0.0, FADE_TIME_MS);
    }
    RadioStream& new_station = m_stations[new_idx];
    if (!new_station.isMuted()) {
        fade_audio(new_station, new_station.getCurrentVolume(), 100.0, FADE_TIME_MS);
    }
    m_active_station_idx = new_idx;
}

void RadioPlayer::toggle_mute_station(int station_idx) {
    RadioStream& station = m_stations[station_idx];
    if (station.isMuted()) {
        station.setMuted(false);
        fade_audio(station, station.getCurrentVolume(), station.getPreMuteVolume(), FADE_TIME_MS / 2);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setMuted(true);
        fade_audio(station, station.getCurrentVolume(), 0.0, FADE_TIME_MS / 2);
    }
}

void RadioPlayer::fade_audio(RadioStream& station, double from_vol, double to_vol, int duration_ms) {
    station.setFading(true);
    station.setTargetVolume(to_vol);
    std::thread([this, &station, from_vol, to_vol, duration_ms]() {
        const int steps = 50;
        const int step_delay = duration_ms > 0 ? duration_ms / steps : 0;
        const double vol_step = (steps > 0) ? (to_vol - from_vol) / steps : (to_vol - from_vol);
        double current_vol = from_vol;
        for (int i = 0; i < steps && !m_quit_flag; ++i) {
            current_vol += vol_step;
            station.setCurrentVolume(current_vol);
            double clamped_vol = std::max(0.0, std::min(100.0, current_vol));
            mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &clamped_vol);
            std::this_thread::sleep_for(std::chrono::milliseconds(step_delay));
        }
        if (!m_quit_flag) {
            station.setCurrentVolume(to_vol);
            double final_vol = std::max(0.0, std::min(100.0, to_vol));
            mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &final_vol);
        }
        station.setFading(false);
    }).detach();
}

void RadioPlayer::mpv_event_loop() {
    while (!m_quit_flag) {
        bool event_found = false;
        for (auto& station : m_stations) {
            if (!station.getMpvHandle()) continue;
            mpv_event* event = mpv_wait_event(station.getMpvHandle(), 0.001);
            if (event && event->event_id != MPV_EVENT_NONE) {
                handle_mpv_event(event);
                event_found = true;
            }
        }
        if (!event_found) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

RadioStream* RadioPlayer::find_station_by_id(int station_id) {
    auto it = std::find_if(m_stations.begin(), m_stations.end(),
                           [station_id](const RadioStream& s) { return s.getID() == station_id; });
    return (it != m_stations.end()) ? &(*it) : nullptr;
}

void RadioPlayer::on_title_changed(RadioStream& station, const std::string& new_title) {
    if (new_title == station.getCurrentTitle() || new_title.empty() || new_title == "N/A" || new_title == "Initializing...") {
        if (new_title != station.getCurrentTitle() && !new_title.empty()) {
             station.setCurrentTitle(new_title);
        }
        return;
    }
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    char time_buf[100];
    std::strftime(time_buf, sizeof(time_buf), "%H:%M", std::localtime(&now_c));
    json song_entry = { std::string(time_buf), new_title };
    {
        std::lock_guard<std::mutex> lock(m_history_mutex);
        (*m_song_history)[station.getName()].push_back(song_entry);
    }
    save_history_to_disk();
    station.setCurrentTitle(new_title);
}

void RadioPlayer::on_stream_eof(RadioStream& station) {
    station.setCurrentTitle("Stream Error - Reconnecting...");
    const char* cmd[] = {"loadfile", station.getURL().c_str(), "replace", nullptr};
    check_mpv_error(mpv_command_async(station.getMpvHandle(), 0, cmd), "reconnect on eof");
}

void RadioPlayer::handle_mpv_event(mpv_event* event) {
    // We only care about property change events with a valid station
    if (event->event_id != MPV_EVENT_PROPERTY_CHANGE) return;
    mpv_event_property* prop = (mpv_event_property*)event->data;
    RadioStream* station = find_station_by_id(event->reply_userdata);
    if (!station) return;
    
    if (strcmp(prop->name, "media-title") == 0 && prop->format == MPV_FORMAT_STRING) {
        char* title_cstr = *(char**)prop->data;
        on_title_changed(*station, title_cstr ? title_cstr : "N/A");
    } else if (strcmp(prop->name, "eof-reached") == 0 && prop->format == MPV_FORMAT_FLAG) {
        if (*(int*)prop->data) {
            on_stream_eof(*station);
        }
    }
}

void RadioPlayer::load_history_from_disk() {
    std::ifstream i("radio_history.json");
    if (i.is_open()) {
        try {
            i >> *m_song_history;
            if (!m_song_history->is_object()) {
                *m_song_history = json::object();
            }
        } catch (...) {
            *m_song_history = json::object();
        }
    }
    for (const auto& station : m_stations) {
        if (!m_song_history->contains(station.getName())) {
            (*m_song_history)[station.getName()] = json::array();
        }
    }
}

void RadioPlayer::save_history_to_disk() {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    std::ofstream o("radio_history.json");
    if (o.is_open()) {
        o << std::setw(4) << *m_song_history << std::endl;
    }
}
