// src/RadioPlayer.cpp
#include "RadioPlayer.h"
#include "UIManager.h"
#include "nlohmann/json.hpp"
#include "Utils.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ncurses.h>

#define FADE_TIME_MS 900
#define SMALL_MODE_TOTAL_TIME_SECONDS 1125
#define DISCOVERY_MODE_REFRESH_MS 1000 // Refresh rate for discovery mode
#define NORMAL_MODE_REFRESH_MS 100    // Refresh rate for normal navigation

using nlohmann::json;

// Helper function to check if a string contains another string, case-insensitively
bool contains_ci(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
    return (it != haystack.end());
}


RadioPlayer::RadioPlayer(std::vector<std::pair<std::string, std::string>> station_data)
    : m_ui(std::make_unique<UIManager>()),
      m_active_station_idx(0),
      m_quit_flag(false),
      m_small_mode_active(false),
      m_active_panel(ActivePanel::STATIONS),
      m_history_scroll_offset(0),
      m_song_history(std::make_unique<json>(json::object())),
      m_small_mode_start_time(std::chrono::steady_clock::now()),
      m_station_switch_duration(0)
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
            m_ui->draw(m_stations, m_active_station_idx, *m_song_history, m_active_panel, m_history_scroll_offset, 
                       m_small_mode_active, get_remaining_seconds_for_current_station(), m_station_switch_duration);
            needs_redraw = false;
        }
        
        // *** THIS IS THE CHANGE: Check for buffering state changes to trigger redraw ***
        if (update_state()) {
            needs_redraw = true;
        }
        for (const auto& station : m_stations) {
            if (station.isBuffering()) { // A bit naive, but will force redraw
                needs_redraw = true;
                break;
            }
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

void RadioPlayer::on_key_enter() {
    if (!m_stations.empty()) {
        toggle_mute_station(m_active_station_idx);
    }
}

void RadioPlayer::handle_input(int ch) {
    switch (ch) {
        case KEY_UP:
            if (m_active_panel == ActivePanel::STATIONS) {
                if (m_active_station_idx > 0) {
                    switch_station(m_active_station_idx - 1);
                }
            } else if (m_active_panel == ActivePanel::HISTORY) {
                if (m_history_scroll_offset > 0) {
                    m_history_scroll_offset--;
                }
            }
            break;

        case KEY_DOWN:
            if (m_active_panel == ActivePanel::STATIONS) {
                if (m_active_station_idx < static_cast<int>(m_stations.size()) - 1) {
                    switch_station(m_active_station_idx + 1);
                }
            } else if (m_active_panel == ActivePanel::HISTORY) {
                const auto& current_station_name = m_stations[m_active_station_idx].getName();
                if (m_song_history->contains(current_station_name)) {
                    const auto& history_list = (*m_song_history)[current_station_name];
                    if (m_history_scroll_offset < (int)history_list.size() - 1) {
                        m_history_scroll_offset++;
                    }
                }
            }
            break;

        case '\t':
            if (m_active_panel == ActivePanel::STATIONS) {
                m_active_panel = ActivePanel::HISTORY;
            } else {
                m_active_panel = ActivePanel::STATIONS;
            }
            break;

        case '\n': case '\r': case KEY_ENTER:
            on_key_enter();
            break;

        case 's': case 'S':
            toggle_small_mode();
            break;

        case 'f': case 'F':
            if (!m_stations.empty()) {
                m_stations[m_active_station_idx].toggleFavorite();
            }
            break;

        case 'q': case 'Q':
            m_quit_flag = true;
            break;
    }
}

void RadioPlayer::toggle_small_mode() {
    m_small_mode_active = !m_small_mode_active;
    if (m_small_mode_active) {
        m_ui->setInputTimeout(DISCOVERY_MODE_REFRESH_MS);
        m_small_mode_start_time = std::chrono::steady_clock::now();
        if(!m_stations.empty()) {
            RadioStream& current_station = m_stations[m_active_station_idx];
            if (current_station.isMuted()) {
                toggle_mute_station(m_active_station_idx);
            } else if (current_station.getCurrentVolume() < 50.0) {
                fade_audio(current_station, current_station.getCurrentVolume(), 100.0, FADE_TIME_MS);
            }
        }
    } else {
        m_ui->setInputTimeout(NORMAL_MODE_REFRESH_MS);
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
    if (new_idx < 0 || new_idx >= (int)m_stations.size() || new_idx == m_active_station_idx) return;
    
    RadioStream& current_station = m_stations[m_active_station_idx];
    if (!current_station.isMuted()) {
        fade_audio(current_station, current_station.getCurrentVolume(), 0.0, FADE_TIME_MS);
    }
    
    RadioStream& new_station = m_stations[new_idx];
    if (!new_station.isMuted()) {
        fade_audio(new_station, new_station.getCurrentVolume(), 100.0, FADE_TIME_MS);
    }
    m_active_station_idx = new_idx;
    
    m_history_scroll_offset = 0;
}

void RadioPlayer::toggle_mute_station(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
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
    if (new_title.empty() || new_title == station.getCurrentTitle() || new_title == "N/A" || new_title == "Initializing...") {
        return;
    }

    if (contains_ci(station.getURL(), new_title) || contains_ci(station.getName(), new_title)) {
        if (new_title != station.getCurrentTitle()) {
             station.setCurrentTitle(new_title);
        }
        return;
    }

    std::string title_to_log = new_title;
    if (!station.hasLoggedFirstSong()) {
        title_to_log = "✨ " + title_to_log;
        station.setHasLoggedFirstSong(true);
    }
    
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    char full_time_buf[100];
    std::strftime(full_time_buf, sizeof(full_time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c));
    
    json history_entry_for_file = { std::string(full_time_buf), title_to_log };
    
    {
        std::lock_guard<std::mutex> lock(m_history_mutex);
        (*m_song_history)[station.getName()].push_back(history_entry_for_file);
    }
    save_history_to_disk();
    station.setCurrentTitle(new_title);
}

void RadioPlayer::on_stream_eof(RadioStream& station) {
    station.setCurrentTitle("Stream Error - Reconnecting...");
    station.setHasLoggedFirstSong(false);
    const char* cmd[] = {"loadfile", station.getURL().c_str(), "replace", nullptr};
    check_mpv_error(mpv_command_async(station.getMpvHandle(), 0, cmd), "reconnect on eof");
}

void RadioPlayer::handle_mpv_event(mpv_event* event) {
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
    } else if (strcmp(prop->name, "core-idle") == 0 && prop->format == MPV_FORMAT_FLAG) { // *** THIS IS THE CHANGE ***
        bool is_idle = *(int*)prop->data;
        station->setBuffering(is_idle);
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
