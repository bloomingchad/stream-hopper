#include "StationManager.h"
#include "PersistenceManager.h"
#include "Core/MpvEventHandler.h" 
#include "Utils.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cmath>
#include <mpv/client.h>

namespace {
    constexpr int FADE_TIME_MS = 900;
    constexpr double DUCK_VOLUME = 40.0;
    // The actor loop will wake up this often to check for animations
    constexpr auto ACTOR_LOOP_TIMEOUT = std::chrono::milliseconds(20);
}

StationManager::StationManager(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data, AppState& app_state)
: m_app_state(app_state)
{
    if (station_data.empty()) {
        throw std::runtime_error("No radio stations provided.");
    }

    for (size_t i = 0; i < station_data.size(); ++i) {
        m_stations.emplace_back(i, station_data[i].first, station_data[i].second.front());
    }
    
    PersistenceManager persistence;
    m_app_state.setHistory(persistence.loadHistory());
    const auto favorite_names = persistence.loadFavoriteNames();
    for (auto& station : m_stations) {
        if (favorite_names.count(station.getName())) {
            station.toggleFavorite();
        }
    }
    if (auto last_station_name = persistence.loadLastStationName()) {
        auto it = std::find_if(m_stations.begin(), m_stations.end(),
                               [&](const RadioStream& station) { return station.getName() == *last_station_name; });
        if (it != m_stations.end()) {
            m_app_state.active_station_idx = std::distance(m_stations.begin(), it);
        }
    }
    for (const auto& station : m_stations) {
        m_app_state.ensureStationHistoryExists(station.getName());
    }

    m_event_handler = std::make_unique<MpvEventHandler>(
        m_stations,
        m_app_state,
        [this](StationManagerMessage msg){ this->post(std::move(msg)); }
    );

    m_actor_thread = std::thread(&StationManager::actorLoop, this);
}

StationManager::~StationManager() {
    post(Msg::Shutdown{});
    if (m_actor_thread.joinable()) {
        m_actor_thread.join();
    }

    PersistenceManager persistence;
    persistence.saveHistory(m_app_state.getFullHistory());
    persistence.saveFavorites(m_stations);
    if (!m_stations.empty()) {
        persistence.saveSession(m_stations[m_app_state.active_station_idx].getName());
    }
}

std::vector<StationDisplayData> StationManager::getStationDisplayData() const {
    std::lock_guard<std::mutex> lock(m_stations_mutex);
    std::vector<StationDisplayData> display_data;
    display_data.reserve(m_stations.size());

    for (const auto& station : m_stations) {
        display_data.push_back({
            .name = station.getName(),
            .current_title = station.getCurrentTitle(),
            .bitrate = station.getBitrate(),
            .current_volume = station.getCurrentVolume(),
            .is_initialized = station.isInitialized(),
            .is_favorite = station.isFavorite(),
            .is_buffering = station.isBuffering(),
            .playback_state = station.getPlaybackState()
        });
    }
    return display_data;
}


void StationManager::post(StationManagerMessage message) {
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_message_queue.push_back(std::move(message));
    }
    m_queue_cond.notify_one();
}

void StationManager::actorLoop() {
    post(Msg::UpdateActiveWindow{});

    bool running = true;
    while(running) {
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            // Wait for a message OR a timeout
            m_queue_cond.wait_for(lock, ACTOR_LOOP_TIMEOUT, [this] { 
                return !m_message_queue.empty(); 
            });
        }
        
        // Always lock station data when we're on the actor thread.
        std::lock_guard<std::mutex> lock(m_stations_mutex);

        // Process all pending messages first
        while(!m_message_queue.empty()) {
            auto msg = std::move(m_message_queue.front());
            m_message_queue.pop_front();

            // Check for shutdown message to exit the loop
            if (std::holds_alternative<Msg::Shutdown>(msg)) {
                handle_shutdown();
                running = false;
                break; 
            }

            std::visit([this](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if      constexpr (std::is_same_v<T, Msg::SwitchStation>)      { handle_switchStation(arg.old_idx, arg.new_idx); }
                else if constexpr (std::is_same_v<T, Msg::ToggleMute>)         { handle_toggleMute(arg.station_idx); }
                else if constexpr (std::is_same_v<T, Msg::ToggleDucking>)     { handle_toggleDucking(arg.station_idx); }
                else if constexpr (std::is_same_v<T, Msg::ToggleFavorite>)    { handle_toggleFavorite(arg.station_idx); }
                else if constexpr (std::is_same_v<T, Msg::SetHopperMode>)      { handle_setHopperMode(arg.new_mode); }
                else if constexpr (std::is_same_v<T, Msg::UpdateActiveWindow>) { handle_updateActiveWindow(); }
                else if constexpr (std::is_same_v<T, Msg::SaveHistory>)       { handle_saveHistory(); }
            }, msg);
        }

        if (!running) break;

        // After messages, process continuous tasks
        handle_activeFades();
        pollMpvEvents();
    }
}

// New method to process fade animations
void StationManager::handle_activeFades() {
    if (m_active_fades.empty()) return;

    auto now = std::chrono::steady_clock::now();
    bool needs_redraw = false;

    // Iterate backwards to allow safe removal
    for (int i = m_active_fades.size() - 1; i >= 0; --i) {
        auto& fade = m_active_fades[i];
        
        RadioStream* station = nullptr;
        if(fade.station_id >=0 && fade.station_id < (int)m_stations.size()){
            station = &m_stations[fade.station_id];
        }

        if (!station || !station->isInitialized() || station->getGeneration() != fade.generation) {
            // Station is gone or has been recycled, remove the fade
            m_active_fades.erase(m_active_fades.begin() + i);
            continue;
        }

        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - fade.start_time).count();
        double progress = 1.0;
        if (fade.duration_ms > 0) {
            progress = std::min(1.0, static_cast<double>(elapsed_ms) / fade.duration_ms);
        }

        double new_vol = fade.start_vol + (fade.target_vol - fade.start_vol) * progress;
        station->setCurrentVolume(new_vol);
        
        double clamped_vol = std::max(0.0, std::min(100.0, new_vol));
        mpv_set_property_async(station->getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &clamped_vol);
        needs_redraw = true;

        if (progress >= 1.0) {
            // Fade is complete
            station->setCurrentVolume(fade.target_vol);
            station->setFading(false);
            m_active_fades.erase(m_active_fades.begin() + i);
        }
    }

    if (needs_redraw) {
        m_app_state.needs_redraw = true;
    }
}

void StationManager::handle_saveHistory() {
    PersistenceManager persistence;
    persistence.saveHistory(m_app_state.getFullHistory());
    m_app_state.unsaved_history_count = 0;
}

void StationManager::handle_shutdown() {
    for (int station_idx : m_active_station_indices) {
        if(station_idx >=0 && station_idx < (int)m_stations.size()){
            m_stations[station_idx].shutdown();
        }
    }
    m_active_station_indices.clear();
    m_active_fades.clear();
}

void StationManager::pollMpvEvents() {
    bool events_pending = true;
    while(events_pending) {
        events_pending = false;
        // Make a copy of indices to avoid issues if m_active_station_indices is modified
        const auto indices_to_poll = m_active_station_indices;
        for(int station_idx : indices_to_poll) { 
            if (station_idx >= (int)m_stations.size() || !m_stations[station_idx].isInitialized()) {
                continue;
            }
            RadioStream& station = m_stations[station_idx];
            mpv_event *event = mpv_wait_event(station.getMpvHandle(), 0);
            if (event->event_id != MPV_EVENT_NONE) {
                m_event_handler->handleEvent(event);
                events_pending = true;
            }
        }
    }
}

void StationManager::handle_switchStation(int old_idx, int new_idx) {
    if (new_idx < 0 || new_idx >= (int)m_stations.size()) return;
    if (m_app_state.hopper_mode == HopperMode::FOCUS) {
        m_app_state.hopper_mode = HopperMode::BALANCED;
    }
    if (new_idx != old_idx) {
        if(old_idx >= 0 && old_idx < (int)m_stations.size()) {
            RadioStream& current_station = m_stations[old_idx];
            if (current_station.isInitialized() && current_station.getPlaybackState() != PlaybackState::Muted) {
                fadeAudio(old_idx, 0.0, FADE_TIME_MS);
            }
        }
        m_app_state.session_switches++;
        m_app_state.last_switch_time = std::chrono::steady_clock::now();
    }
}

void StationManager::handle_updateActiveWindow() {
    const auto new_active_set = m_preloader.calculate_active_indices(
        m_app_state.active_station_idx, m_stations.size(),
        m_app_state.hopper_mode, m_app_state.nav_history);
    std::vector<int> to_shutdown;
    for (int idx : m_active_station_indices) {
        if (new_active_set.find(idx) == new_active_set.end()) {
            to_shutdown.push_back(idx);
        }
    }
    for (int idx : to_shutdown) shutdownStation(idx);
    for (int idx : new_active_set) {
        if (m_active_station_indices.find(idx) == m_active_station_indices.end()) {
            initializeStation(idx);
        }
    }
    
    int active_idx = m_app_state.active_station_idx;
    if(active_idx >= 0 && active_idx < (int)m_stations.size()) {
        RadioStream& new_station = m_stations[active_idx];
        if (new_station.isInitialized() && new_station.getPlaybackState() != PlaybackState::Muted && new_station.getCurrentVolume() < 99.0) {
            fadeAudio(active_idx, 100.0, FADE_TIME_MS);
        }
    }
    m_app_state.needs_redraw = true;
}

void StationManager::handle_toggleMute(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    RadioStream& station = m_stations[station_idx];
    if (!station.isInitialized() || station.getPlaybackState() == PlaybackState::Ducked) return;
    if (station.getPlaybackState() == PlaybackState::Muted) {
        station.setPlaybackState(PlaybackState::Playing);
        station.resetMuteStartTime();
        fadeAudio(station_idx, station.getPreMuteVolume(), FADE_TIME_MS / 2);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setPlaybackState(PlaybackState::Muted);
        station.setMuteStartTime();
        fadeAudio(station_idx, 0.0, FADE_TIME_MS / 2);
    }
}

void StationManager::handle_toggleDucking(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    RadioStream& station = m_stations[station_idx];
    if (!station.isInitialized() || station.getPlaybackState() == PlaybackState::Muted) return;
    if (station.getPlaybackState() == PlaybackState::Ducked) {
        station.setPlaybackState(PlaybackState::Playing);
        fadeAudio(station_idx, station.getPreMuteVolume(), FADE_TIME_MS);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setPlaybackState(PlaybackState::Ducked);
        fadeAudio(station_idx, DUCK_VOLUME, FADE_TIME_MS);
    }
}

void StationManager::handle_toggleFavorite(int station_idx) {
    if (station_idx >= 0 && station_idx < (int)m_stations.size()) {
        m_stations[station_idx].toggleFavorite();
    }
}

void StationManager::handle_setHopperMode(HopperMode new_mode) {
    if (m_app_state.hopper_mode != new_mode) {
        m_app_state.hopper_mode = new_mode;
        handle_updateActiveWindow();
    }
}

void StationManager::initializeStation(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    double vol = (station_idx == m_app_state.active_station_idx) ? 100.0 : 0.0;
    m_stations[station_idx].initialize(vol);
    m_active_station_indices.insert(station_idx);
}

void StationManager::shutdownStation(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    m_stations[station_idx].shutdown();
    m_active_station_indices.erase(station_idx);
}

// New fadeAudio simply creates an ActiveFade request for the actor loop
void StationManager::fadeAudio(int station_id, double to_vol, int duration_ms) {
    if (station_id < 0 || station_id >= (int)m_stations.size()) return;
    RadioStream& station = m_stations[station_id];
    if (!station.isInitialized()) return;

    // Remove any existing fade for this station to avoid conflicts
    m_active_fades.erase(
        std::remove_if(m_active_fades.begin(), m_active_fades.end(),
                       [station_id](const ActiveFade& f) { return f.station_id == station_id; }),
        m_active_fades.end());
    
    station.setFading(true);
    m_active_fades.push_back({
        .station_id = station_id,
        .generation = station.getGeneration(),
        .start_vol = station.getCurrentVolume(),
        .target_vol = to_vol,
        .start_time = std::chrono::steady_clock::now(),
        .duration_ms = duration_ms
    });
}
