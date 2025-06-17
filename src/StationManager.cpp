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
#include <cmath>
#include <unordered_set>
#include <deque>
#include <mpv/client.h>

namespace {
    constexpr int FADE_TIME_MS = 900;
    constexpr double DUCK_VOLUME = 40.0;
    constexpr int BITRATE_REDRAW_THRESHOLD = 2;
    constexpr int BALANCED_MODE_WINDOW_SIZE = 3;
}

StationManager::StationManager(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data, AppState& app_state)
    : m_app_state(app_state),
      m_wakeup_flag(false) {

    if (station_data.empty()) {
        throw std::runtime_error("No radio stations provided.");
    }

    for (size_t i = 0; i < station_data.size(); ++i) {
        m_stations.emplace_back(i, station_data[i].first, station_data[i].second.front());
    }
    
    m_app_state.loadFavorites(m_stations);
    m_app_state.loadSession(m_stations);
    m_app_state.loadHistoryFromDisk();
    
    for (const auto& station : m_stations) {
        m_app_state.ensureStationHistoryExists(station.getName());
    }
}

StationManager::~StationManager() {
    stopEventLoop();
    
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
    updateActiveWindow();
}

void StationManager::stopEventLoop() {
    if (m_mpv_event_thread.joinable()) {
        m_app_state.quit_flag = true; 
        wakeupEventLoop(); 
        m_mpv_event_thread.join();
    }
}

const std::vector<RadioStream>& StationManager::getStations() const {
    return m_stations;
}

void StationManager::switchStation(int old_idx, int new_idx) {
    if (new_idx < 0 || new_idx >= (int)m_stations.size()) return;

    if (m_app_state.hopper_mode == HopperMode::FOCUS) {
        setHopperMode(HopperMode::BALANCED);
    }
    
    if (new_idx != old_idx) {
        RadioStream& current_station = m_stations[old_idx];
        if (current_station.isInitialized() && current_station.getPlaybackState() != PlaybackState::Muted) {
            fadeAudio(current_station, current_station.getCurrentVolume(), 0.0, FADE_TIME_MS);
        }
        m_app_state.session_switches++;
        m_app_state.last_switch_time = std::chrono::steady_clock::now();
    }
}

void StationManager::toggleMuteStation(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    RadioStream& station = m_stations[station_idx];
    if (!station.isInitialized()) return;
    if (station.getPlaybackState() == PlaybackState::Ducked) return;

    if (station.getPlaybackState() == PlaybackState::Muted) {
        station.setPlaybackState(PlaybackState::Playing);
        station.resetMuteStartTime();
        fadeAudio(station, station.getCurrentVolume(), station.getPreMuteVolume(), FADE_TIME_MS / 2);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setPlaybackState(PlaybackState::Muted);
        station.setMuteStartTime();
        fadeAudio(station, station.getCurrentVolume(), 0.0, FADE_TIME_MS / 2);
    }
}

void StationManager::toggleAudioDucking(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    RadioStream& station = m_stations[station_idx];
    if (!station.isInitialized()) return;
    if (station.getPlaybackState() == PlaybackState::Muted) return;

    if (station.getPlaybackState() == PlaybackState::Ducked) {
        station.setPlaybackState(PlaybackState::Playing);
        fadeAudio(station, station.getCurrentVolume(), station.getPreMuteVolume(), FADE_TIME_MS);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setPlaybackState(PlaybackState::Ducked);
        fadeAudio(station, station.getCurrentVolume(), DUCK_VOLUME, FADE_TIME_MS);
    }
}

void StationManager::toggleFavorite(int station_idx) {
    if (station_idx >= 0 && station_idx < (int)m_stations.size()) {
        m_stations[station_idx].toggleFavorite();
    }
}

void StationManager::setHopperMode(HopperMode new_mode) {
    if (m_app_state.hopper_mode != new_mode) {
        m_app_state.hopper_mode = new_mode;
        updateActiveWindow();
        m_app_state.needs_redraw = true;
    }
}

void StationManager::updateActiveWindow() {
    std::unordered_set<int> new_active_set;
    int station_count = static_cast<int>(m_stations.size());
    int active_idx = m_app_state.active_station_idx;

    switch (m_app_state.hopper_mode) {
        case HopperMode::PERFORMANCE:
            for (int i = 0; i < station_count; ++i) new_active_set.insert(i);
            break;
        case HopperMode::FOCUS:
            new_active_set.insert(active_idx);
            break;
        case HopperMode::BALANCED:
            new_active_set.insert(active_idx);
            for (int i = 1; i <= BALANCED_MODE_WINDOW_SIZE; ++i) {
                new_active_set.insert((active_idx + i) % station_count);
                new_active_set.insert((active_idx - i + station_count) % station_count);
            }
            break;
    }

    std::lock_guard<std::mutex> lock(m_active_indices_mutex); 
    
    for (int i = 0; i < station_count; ++i) {
        bool should_be_active = new_active_set.count(i);
        bool is_active = m_active_station_indices.count(i);

        if (should_be_active && !is_active) {
            requestStationInitialization(i);
        } else if (!should_be_active && is_active) {
            requestStationShutdown(i);
        }
    }

    RadioStream& new_station = m_stations[active_idx];
    if (new_station.isInitialized() && new_station.getPlaybackState() != PlaybackState::Muted && new_station.getCurrentVolume() < 99.0) {
        fadeAudio(new_station, new_station.getCurrentVolume(), 100.0, FADE_TIME_MS);
    }
    wakeupEventLoop();
}

void StationManager::requestStationInitialization(int station_idx) {
    std::lock_guard<std::mutex> lock(m_lifecycle_queue_mutex);
    m_initialization_queue.push_back(station_idx);
}

void StationManager::requestStationShutdown(int station_idx) {
    std::lock_guard<std::mutex> lock(m_lifecycle_queue_mutex);
    m_shutdown_queue.push_back(station_idx);
}

void StationManager::processLifecycleRequests() {
    // Create local copies of the queues to process.
    std::deque<int> initializations_to_process;
    std::deque<int> shutdowns_to_process;

    // --- The Fix: Lock, copy, and immediately unlock ---
    {
        std::lock_guard<std::mutex> lock(m_lifecycle_queue_mutex);
        if (!m_initialization_queue.empty()) {
            initializations_to_process.swap(m_initialization_queue);
        }
        if (!m_shutdown_queue.empty()) {
            shutdowns_to_process.swap(m_shutdown_queue);
        }
    } // Mutex is released here.

    // Now, process the requests using the local copies, without holding the lock.

    // Process shutdowns first.
    if (!shutdowns_to_process.empty()) {
        std::lock_guard<std::mutex> index_lock(m_active_indices_mutex);
        for (int station_idx : shutdowns_to_process) {
            if (station_idx >= 0 && station_idx < (int)m_stations.size()) {
                m_stations[station_idx].shutdown();
                m_active_station_indices.erase(station_idx);
            }
        }
    }
    
    // Process initializations.
    if (!initializations_to_process.empty()) {
        std::lock_guard<std::mutex> index_lock(m_active_indices_mutex);
        for (int station_idx : initializations_to_process) {
            if (station_idx >= 0 && station_idx < (int)m_stations.size()) {
                double initial_volume = (station_idx == m_app_state.active_station_idx) ? 100.0 : 0.0;
                m_stations[station_idx].initialize(initial_volume);
                if (m_stations[station_idx].getMpvHandle()) {
                    mpv_set_wakeup_callback(m_stations[station_idx].getMpvHandle(), mpv_wakeup_cb, this);
                }
                m_active_station_indices.insert(station_idx);
            }
        }
    }
}

void StationManager::wakeupEventLoop() {
    m_wakeup_flag = true;
    m_event_cond.notify_one();
}

void StationManager::mpv_wakeup_cb(void* ctx) {
    static_cast<StationManager*>(ctx)->wakeupEventLoop();
}

void StationManager::fadeAudio(RadioStream& station, double from_vol, double to_vol, int duration_ms) {
    if (!station.isInitialized()) return;
    station.setFading(true);
    station.setTargetVolume(to_vol);

    // --- The Fix Part 2 ---
    // Capture the generation number at the time the thread is created.
    int captured_generation = station.getGeneration();

    std::thread fade_worker([this, &station, from_vol, to_vol, duration_ms, captured_generation]() {
        const int steps = 50;
        const int step_delay_ms = duration_ms > 0 ? duration_ms / steps : 0;
        const double vol_step = (steps > 0) ? (to_vol - from_vol) / steps : (to_vol - from_vol);
        double current_vol = from_vol;

        for (int i = 0; i < steps; ++i) {
            // --- The Fix Part 3 ---
            // The check is now against the captured generation. This is thread-safe.
            if (m_app_state.quit_flag || station.getGeneration() != captured_generation || std::abs(station.getTargetVolume() - to_vol) > 0.01) {
                return; // The station was shut down or another fade started. Exit.
            }
            current_vol += vol_step;
            station.setCurrentVolume(current_vol);
            double clamped_vol = std::max(0.0, std::min(100.0, current_vol));
            if (station.getMpvHandle()) {
                mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &clamped_vol);
            }
            wakeupEventLoop();
            std::this_thread::sleep_for(std::chrono::milliseconds(step_delay_ms));
        }

        if (!m_app_state.quit_flag && station.getGeneration() == captured_generation && std::abs(station.getTargetVolume() - to_vol) <= 0.01) {
            station.setCurrentVolume(to_vol);
            double final_vol = std::max(0.0, std::min(100.0, to_vol));
            if (station.getMpvHandle()) {
               mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &final_vol);
            }
        }
        if (station.getGeneration() == captured_generation) {
             station.setFading(false);
        }
        wakeupEventLoop();
    });

    std::lock_guard<std::mutex> lock(m_fade_threads_mutex);
    m_fade_threads.push_back(std::move(fade_worker));
}

void StationManager::mpvEventLoop() {
    while (!m_app_state.quit_flag.load()) {
        {
            std::unique_lock<std::mutex> lock(m_event_mutex);
            m_event_cond.wait(lock, [this]{ return m_wakeup_flag.load(); });
            m_wakeup_flag = false;
        }

        if (m_app_state.quit_flag.load()) break;

        processLifecycleRequests();

        std::unordered_set<int> indices_to_poll;
        {
            std::lock_guard<std::mutex> lock(m_active_indices_mutex);
            indices_to_poll = m_active_station_indices;
        }

        bool events_pending = true;
        while(events_pending) {
            events_pending = false;
            for(int station_idx : indices_to_poll) { 
                if (station_idx >= (int)m_stations.size() || !m_stations[station_idx].isInitialized()) {
                    continue;
                }
                
                RadioStream& station = m_stations[station_idx];
                mpv_event *event = mpv_wait_event(station.getMpvHandle(), 0);
                if (event->event_id != MPV_EVENT_NONE) {
                    handleMpvEvent(event);
                    events_pending = true;
                }
            }
        }
    }
}

void StationManager::handleMpvEvent(mpv_event* event) {
    if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
        handlePropertyChange(event);
    }
}

void StationManager::handlePropertyChange(mpv_event* event) {
    mpv_event_property* prop = reinterpret_cast<mpv_event_property*>(event->data);
    RadioStream* station = findStationById(event->reply_userdata);
    if (!station || !station->isInitialized()) return;
    
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

void StationManager::cleanupFinishedThreads() {
    std::lock_guard<std::mutex> lock(m_fade_threads_mutex);
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
    m_app_state.new_songs_found++;
    
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
