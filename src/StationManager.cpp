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
#include <cmath> // For std::abs
#include <mpv/client.h> // For mpv_event definitions


namespace {
    constexpr int FADE_TIME_MS = 900;
    constexpr double DUCK_VOLUME = 40.0;
    constexpr int BITRATE_REDRAW_THRESHOLD = 2;
}

StationManager::StationManager(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data, AppState& app_state)
    : m_app_state(app_state) {

    if (station_data.empty()) {
        throw std::runtime_error("No radio stations provided.");
    }

    // Create RadioStream objects
    for (size_t i = 0; i < station_data.size(); ++i) {
        m_stations.emplace_back(i, station_data[i].first, station_data[i].second.front());
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
        m_app_state.ensureStationHistoryExists(station.getName());
    }
}

StationManager::~StationManager() {
    // Gracefully stop all threads before saving state
    stopEventLoop(); // Stops the main mpv event loop
    
    // Join all fade threads before exiting. The m_app_state.quit_flag
    // (set in RadioPlayer's destructor) will cause them to terminate quickly.
    for (auto& t : m_fade_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    m_app_state.saveFavorites(m_stations);
    m_app_state.saveSession(m_stations);
    m_app_state.saveHistoryToDisk();
}

void StationManager::runEventLoop() {
    m_mpv_event_thread = std::thread(&StationManager::mpvEventLoop, this);
}

void StationManager::stopEventLoop() {
    // Check if the thread was ever started before trying to join
    if (m_mpv_event_thread.joinable()) {
        // Set the quit flag which the loop checks
        m_app_state.quit_flag = true; 
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

    m_app_state.session_switches++; // <-- INCREMENT SWITCH COUNTER
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
    }
}

// --- Private Implementation ---

void StationManager::cleanupFinishedThreads() {
    std::lock_guard<std::mutex> lock(m_fade_threads_mutex);
}

void StationManager::fadeAudio(RadioStream& station, double from_vol, double to_vol, int duration_ms) {
    // cleanupFinishedThreads(); // Disabling pruning to avoid blocking UI. See function comment.

    station.setFading(true);
    station.setTargetVolume(to_vol);

    std::thread fade_worker([this, &station, from_vol, to_vol, duration_ms]() {
        const int steps = 50;
        const int step_delay_ms = duration_ms > 0 ? duration_ms / steps : 0;
        const double vol_step = (steps > 0) ? (to_vol - from_vol) / steps : (to_vol - from_vol);
        double current_vol = from_vol;

        for (int i = 0; i < steps; ++i) {
            // Exit if app is quitting or if another fade has superseded this one.
            if (m_app_state.quit_flag || std::abs(station.getTargetVolume() - to_vol) > 0.01) {
                return;
            }

            current_vol += vol_step;
            station.setCurrentVolume(current_vol);
            double clamped_vol = std::max(0.0, std::min(100.0, current_vol));
            if(station.getMpvHandle()) { // Safety check
                mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &clamped_vol);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(step_delay_ms));
        }

        // Only set the final state if this thread completed its fade successfully.
        if (!m_app_state.quit_flag && std::abs(station.getTargetVolume() - to_vol) <= 0.01) {
            station.setCurrentVolume(to_vol);
            double final_vol = std::max(0.0, std::min(100.0, to_vol));
            if(station.getMpvHandle()) { // Safety check
               mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &final_vol);
            }
        }
        station.setFading(false);
    });

    // Move the thread into our managed vector
    std::lock_guard<std::mutex> lock(m_fade_threads_mutex);
    m_fade_threads.push_back(std::move(fade_worker));
}

void StationManager::mpvEventLoop() {
    while (!m_app_state.quit_flag) {
        bool event_found = false;
        // The check for m_app_state.quit_flag must be inside the loop
        // for it to terminate correctly when stopEventLoop() is called.
        for (auto& station : m_stations) {
            if (m_app_state.quit_flag) break;
            if (!station.getMpvHandle()) continue;
            mpv_event* event = mpv_wait_event(station.getMpvHandle(), 0.001);
            if (event && event->event_id != MPV_EVENT_NONE) {
                handleMpvEvent(event);
                event_found = true;
            }
        }
        if (!event_found && !m_app_state.quit_flag) {
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
    
    m_app_state.addHistoryEntry(station.getName(), history_entry_for_file);
    m_app_state.new_songs_found++; // <-- INCREMENT NEW SONG COUNTER
    
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
    if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
        handlePropertyChange(event);
    }
    // Other event types like MPV_EVENT_SHUTDOWN could be handled here
}

void StationManager::handlePropertyChange(mpv_event* event) {
    mpv_event_property* prop = reinterpret_cast<mpv_event_property*>(event->data);
    RadioStream* station = findStationById(event->reply_userdata);
    if (!station) return;
    
    if (strcmp(prop->name, "media-title") == 0) {
        onTitleProperty(prop, *station);
    } else if (strcmp(prop->name, "audio-bitrate") == 0) {
        onBitrateProperty(prop, *station);
    } else if (strcmp(prop->name, "eof-reached") == 0) {
        onEofProperty(prop, *station);
    } else if (strcmp(prop->name, "core-idle") == 0) {
        onCoreIdleProperty(prop, *station);
    }
}

void StationManager::onTitleProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_STRING) {
        char* title_cstr = *reinterpret_cast<char**>(prop->data);
        onTitleChanged(station, title_cstr ? title_cstr : "N/A");
    }
}

void StationManager::onBitrateProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_INT64) {
        int old_bitrate = station.getBitrate();
        int new_bitrate = static_cast<int>(*reinterpret_cast<int64_t*>(prop->data) / 1000);
        station.setBitrate(new_bitrate);
        
        bool is_active_station = (station.getID() == m_app_state.active_station_idx);
        if (is_active_station && std::abs(new_bitrate - old_bitrate) > BITRATE_REDRAW_THRESHOLD) {
            m_app_state.needs_redraw = true;
        }
    }
}

void StationManager::onEofProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_FLAG && *reinterpret_cast<int*>(prop->data)) {
        onStreamEof(station);
    }
}

void StationManager::onCoreIdleProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_FLAG) {
        bool is_idle = *reinterpret_cast<int*>(prop->data);
        if (station.isBuffering() != is_idle) {
            station.setBuffering(is_idle);
            m_app_state.needs_redraw = true;
        }
    }
}
