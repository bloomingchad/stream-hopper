#include "StationManager.h"
#include "AppState.h"
#include "Utils.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <ctime>


// Constants from the old RadioPlayer
#define FADE_TIME_MS 900
#define DUCK_VOLUME 40.0

StationManager::StationManager(const std::vector<std::pair<std::string, std::string>>& station_data, AppState& app_state)
    : m_app_state(app_state) {

    if (station_data.empty()) {
        throw std::runtime_error("No radio stations provided.");
    }

    // Create RadioStream objects
    for (size_t i = 0; i < station_data.size(); ++i) {
        m_stations.emplace_back(i, station_data[i].first, station_data[i].second);
    }
    
    // Load state that affects stations (favorites, last session)
    m_app_state.loadFavorites(m_stations);
    m_app_state.loadSession(m_stations);
    m_app_state.loadHistoryFromDisk();

    // Initialize mpv instances
    for (int i = 0; i < static_cast<int>(m_stations.size()); ++i) {
        double initial_volume = (i == m_app_state.active_station_idx) ? 100.0 : 0.0;
        m_stations[i].initialize(initial_volume);
    }

    // Ensure history is initialized for all stations
    for (const auto& station : m_stations) {
        if (!m_app_state.getHistory().contains(station.getName())) {
            m_app_state.getHistory()[station.getName()] = nlohmann::json::array();
        }
    }
}

StationManager::~StationManager() {
    stopEventLoop();
    m_app_state.saveFavorites(m_stations);
    m_app_state.saveSession(m_stations);
    m_app_state.saveHistoryToDisk();
}

void StationManager::runEventLoop() {
    m_mpv_event_thread = std::thread(&StationManager::mpvEventLoop, this);
}

void StationManager::stopEventLoop() {
    if (m_mpv_event_thread.joinable()) {
        m_mpv_event_thread.join();
    }
}

const std::vector<RadioStream>& StationManager::getStations() const {
    return m_stations;
}

// --- High-Level Actions ---

void StationManager::switchStation(int old_idx, int new_idx) {
    if (new_idx < 0 || new_idx >= (int)m_stations.size() || new_idx == old_idx) return;
    
    RadioStream& current_station = m_stations[old_idx];
    if (!current_station.isMuted()) {
        fadeAudio(current_station, current_station.getCurrentVolume(), 0.0, FADE_TIME_MS);
    }
    
    RadioStream& new_station = m_stations[new_idx];
    if (!new_station.isMuted()) {
        fadeAudio(new_station, new_station.getCurrentVolume(), 100.0, FADE_TIME_MS);
    }
}

void StationManager::toggleMuteStation(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    RadioStream& station = m_stations[station_idx];

    if (station.isDucked()) return;

    if (station.isMuted()) {
        station.setMuted(false);
        station.resetMuteStartTime();
        fadeAudio(station, station.getCurrentVolume(), station.getPreMuteVolume(), FADE_TIME_MS / 2);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setMuted(true);
        station.setMuteStartTime();
        fadeAudio(station, station.getCurrentVolume(), 0.0, FADE_TIME_MS / 2);
    }
}

void StationManager::toggleAudioDucking(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    RadioStream& station = m_stations[station_idx];

    if (station.isMuted()) return;

    if (station.isDucked()) {
        station.setDucked(false);
        fadeAudio(station, station.getCurrentVolume(), station.getPreMuteVolume(), FADE_TIME_MS);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setDucked(true);
        fadeAudio(station, station.getCurrentVolume(), DUCK_VOLUME, FADE_TIME_MS);
    }
}

void StationManager::toggleFavorite(int station_idx) {
    if (station_idx >= 0 && station_idx < (int)m_stations.size()) {
        m_stations[station_idx].toggleFavorite();
        // Saving is handled in destructor or could be done immediately
    }
}

// --- Private Implementation ---

void StationManager::fadeAudio(RadioStream& station, double from_vol, double to_vol, int duration_ms) {
    station.setFading(true);
    station.setTargetVolume(to_vol);
    std::thread([this, &station, from_vol, to_vol, duration_ms]() {
        const int steps = 50;
        const int step_delay = duration_ms > 0 ? duration_ms / steps : 0;
        const double vol_step = (steps > 0) ? (to_vol - from_vol) / steps : (to_vol - from_vol);
        double current_vol = from_vol;
        for (int i = 0; i < steps && !m_app_state.quit_flag; ++i) {
            current_vol += vol_step;
            station.setCurrentVolume(current_vol);
            double clamped_vol = std::max(0.0, std::min(100.0, current_vol));
            mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &clamped_vol);
            std::this_thread::sleep_for(std::chrono::milliseconds(step_delay));
        }
        if (!m_app_state.quit_flag) {
            station.setCurrentVolume(to_vol);
            double final_vol = std::max(0.0, std::min(100.0, to_vol));
            mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &final_vol);
        }
        station.setFading(false);
    }).detach();
}

void StationManager::mpvEventLoop() {
    while (!m_app_state.quit_flag) {
        bool event_found = false;
        for (auto& station : m_stations) {
            if (!station.getMpvHandle()) continue;
            mpv_event* event = mpv_wait_event(station.getMpvHandle(), 0.001);
            if (event && event->event_id != MPV_EVENT_NONE) {
                handleMpvEvent(event);
                event_found = true;
            }
        }
        if (!event_found) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

RadioStream* StationManager::findStationById(int station_id) {
    auto it = std::find_if(m_stations.begin(), m_stations.end(),
                           [station_id](const RadioStream& s) { return s.getID() == station_id; });
    return (it != m_stations.end()) ? &(*it) : nullptr;
}

bool StationManager::contains_ci(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
    return (it != haystack.end());
}

void StationManager::onTitleChanged(RadioStream& station, const std::string& new_title) {
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
    std::tm now_tm = *std::localtime(&now_c);
    std::stringstream time_ss;
    time_ss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
    
    nlohmann::json history_entry_for_file = { time_ss.str(), title_to_log };
    
    m_app_state.getHistory()[station.getName()].push_back(history_entry_for_file);
    
    // Auto-save history on each new entry
    m_app_state.saveHistoryToDisk(); 
    station.setCurrentTitle(new_title);
    m_app_state.needs_redraw = true;
}

void StationManager::onStreamEof(RadioStream& station) {
    station.setCurrentTitle("Stream Error - Reconnecting...");
    station.setHasLoggedFirstSong(false);
    m_app_state.needs_redraw = true;
    const char* cmd[] = {"loadfile", station.getURL().c_str(), "replace", nullptr};
    check_mpv_error(mpv_command_async(station.getMpvHandle(), 0, cmd), "reconnect on eof");
}

void StationManager::handleMpvEvent(mpv_event* event) {
    if (event->event_id != MPV_EVENT_PROPERTY_CHANGE) return;
    mpv_event_property* prop = reinterpret_cast<mpv_event_property*>(event->data);
    RadioStream* station = findStationById(event->reply_userdata);
    if (!station) return;
    
    bool state_changed = false;
    if (strcmp(prop->name, "media-title") == 0 && prop->format == MPV_FORMAT_STRING) {
        char* title_cstr = *reinterpret_cast<char**>(prop->data);
        onTitleChanged(*station, title_cstr ? title_cstr : "N/A");
    } else if (strcmp(prop->name, "eof-reached") == 0 && prop->format == MPV_FORMAT_FLAG) {
        if (*reinterpret_cast<int*>(prop->data)) {
            onStreamEof(*station);
        }
    } else if (strcmp(prop->name, "core-idle") == 0 && prop->format == MPV_FORMAT_FLAG) {
        bool is_idle = *reinterpret_cast<int*>(prop->data);
        if (station->isBuffering() != is_idle) {
            station->setBuffering(is_idle);
            state_changed = true;
        }
    }

    if (state_changed) {
        m_app_state.needs_redraw = true;
    }
}
