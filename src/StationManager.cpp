#include "StationManager.h"
#include "PersistenceManager.h"
#include "Core/MpvEventHandler.h" // NEW: Include the handler
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
}

StationManager::StationManager(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data, AppState& app_state)
    : m_app_state(app_state) {

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

    // NEW: Instantiate the event handler, passing it state and a poster function.
    m_event_handler = std::make_unique<MpvEventHandler>(
        m_stations,
        m_app_state,
        [this](StationManagerMessage msg){ this->post(std::move(msg)); }
    );

    m_actor_thread = std::thread(&StationManager::actorLoop, this);
}

StationManager::~StationManager() {
    // Post the shutdown message and wait for the thread to finish
    post(Msg::Shutdown{});
    if (m_actor_thread.joinable()) {
        m_actor_thread.join();
    }

    // Wait for any outstanding fade threads to complete BEFORE we save
    {
        std::lock_guard<std::mutex> lock(m_fade_futures_mutex);
        for(auto& fut : m_fade_futures) {
            if (fut.valid()) {
                fut.wait();
            }
        }
    }

    // This now acts as the final "flush" for any unsaved history.
    PersistenceManager persistence;
    persistence.saveHistory(m_app_state.getFullHistory());
    persistence.saveFavorites(m_stations);
    if (!m_stations.empty()) {
        persistence.saveSession(m_stations[m_app_state.active_station_idx].getName());
    }
}

const std::vector<RadioStream>& StationManager::getStations() const {
    return m_stations;
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
    while (processNextMessage()) {}
}

bool StationManager::processNextMessage() {
    StationManagerMessage message;
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);
        m_queue_cond.wait(lock, [this] { return !m_message_queue.empty(); });
        message = std::move(m_message_queue.front());
        m_message_queue.pop_front();
    }

    bool should_continue = true;
    std::visit([&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if      constexpr (std::is_same_v<T, Msg::SwitchStation>)      { handle_switchStation(arg.old_idx, arg.new_idx); }
        else if constexpr (std::is_same_v<T, Msg::ToggleMute>)         { handle_toggleMute(arg.station_idx); }
        else if constexpr (std::is_same_v<T, Msg::ToggleDucking>)     { handle_toggleDucking(arg.station_idx); }
        else if constexpr (std::is_same_v<T, Msg::ToggleFavorite>)    { handle_toggleFavorite(arg.station_idx); }
        else if constexpr (std::is_same_v<T, Msg::SetHopperMode>)      { handle_setHopperMode(arg.new_mode); }
        else if constexpr (std::is_same_v<T, Msg::UpdateActiveWindow>) { handle_updateActiveWindow(); }
        else if constexpr (std::is_same_v<T, Msg::SaveHistory>)       { handle_saveHistory(); }
        else if constexpr (std::is_same_v<T, Msg::Shutdown>)          { handle_shutdown(); should_continue = false; }
    }, message);

    if (should_continue) {
        pollMpvEvents();
        cleanupFinishedFutures();
    }

    return should_continue;
}

void StationManager::handle_saveHistory() {
    PersistenceManager persistence;
    persistence.saveHistory(m_app_state.getFullHistory());
    // Reset the counter after a successful save
    m_app_state.unsaved_history_count = 0;
}

void StationManager::handle_shutdown() {
    for (int station_idx : m_active_station_indices) {
        m_stations[station_idx].shutdown();
    }
    m_active_station_indices.clear();
}

// --- REFACTORED: pollMpvEvents now delegates to the handler component ---
void StationManager::pollMpvEvents() {
    bool events_pending = true;
    while(events_pending) {
        events_pending = false;
        const auto& indices_to_poll = m_active_station_indices;
        for(int station_idx : indices_to_poll) { 
            if (station_idx >= (int)m_stations.size() || !m_stations[station_idx].isInitialized()) {
                continue;
            }
            
            RadioStream& station = m_stations[station_idx];
            // We use a non-blocking wait because this is a polling loop
            mpv_event *event = mpv_wait_event(station.getMpvHandle(), 0);
            if (event->event_id != MPV_EVENT_NONE) {
                m_event_handler->handleEvent(event); // DELEGATE!
                events_pending = true;
            }
        }
    }
}

// --- Message Handler Implementations (Private) ---

void StationManager::handle_switchStation(int old_idx, int new_idx) {
    if (new_idx < 0 || new_idx >= (int)m_stations.size()) return;

    if (m_app_state.hopper_mode == HopperMode::FOCUS) {
        handle_setHopperMode(HopperMode::BALANCED);
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

void StationManager::handle_updateActiveWindow() {
    int station_count = static_cast<int>(m_stations.size());

    // --- DELEGATE to the strategy object to determine which stations should be active ---
    const auto new_active_set = m_preloader.calculate_active_indices(
        m_app_state.active_station_idx,
        station_count,
        m_app_state.hopper_mode,
        m_app_state.nav_history);
    
    // Shutdown stations no longer in the active set
    std::vector<int> to_shutdown;
    for (int idx : m_active_station_indices) {
        if (new_active_set.find(idx) == new_active_set.end()) {
            to_shutdown.push_back(idx);
        }
    }
    for (int idx : to_shutdown) {
        shutdownStation(idx);
    }

    // Initialize new stations
    for (int idx : new_active_set) {
        if (m_active_station_indices.find(idx) == m_active_station_indices.end()) {
            initializeStation(idx);
        }
    }

    // Ensure the new station is audible
    int active_idx = m_app_state.active_station_idx;
    RadioStream& new_station = m_stations[active_idx];
    if (new_station.isInitialized() && new_station.getPlaybackState() != PlaybackState::Muted && new_station.getCurrentVolume() < 99.0) {
        fadeAudio(new_station, new_station.getCurrentVolume(), 100.0, FADE_TIME_MS);
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
        fadeAudio(station, station.getCurrentVolume(), station.getPreMuteVolume(), FADE_TIME_MS / 2);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setPlaybackState(PlaybackState::Muted);
        station.setMuteStartTime();
        fadeAudio(station, station.getCurrentVolume(), 0.0, FADE_TIME_MS / 2);
    }
}

void StationManager::handle_toggleDucking(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    RadioStream& station = m_stations[station_idx];
    if (!station.isInitialized() || station.getPlaybackState() == PlaybackState::Muted) return;

    if (station.getPlaybackState() == PlaybackState::Ducked) {
        station.setPlaybackState(PlaybackState::Playing);
        fadeAudio(station, station.getCurrentVolume(), station.getPreMuteVolume(), FADE_TIME_MS);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setPlaybackState(PlaybackState::Ducked);
        fadeAudio(station, station.getCurrentVolume(), DUCK_VOLUME, FADE_TIME_MS);
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

// --- Other Private Helpers ---

void StationManager::initializeStation(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    double initial_volume = (station_idx == m_app_state.active_station_idx) ? 100.0 : 0.0;
    m_stations[station_idx].initialize(initial_volume);
    m_active_station_indices.insert(station_idx);
}

void StationManager::shutdownStation(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    m_stations[station_idx].shutdown();
    m_active_station_indices.erase(station_idx);
}

bool StationManager::isFadeStillValid(const RadioStream& station, int captured_generation, double target_volume) const {
    if (m_app_state.quit_flag) return false;
    if (station.getGeneration() != captured_generation) return false;
    if (std::abs(station.getTargetVolume() - target_volume) > 0.01) return false;
    return true;
}

void StationManager::fadeAudio(RadioStream& station, double from_vol, double to_vol, int duration_ms) {
    if (!station.isInitialized()) return;
    station.setFading(true);
    station.setTargetVolume(to_vol);
    int captured_generation = station.getGeneration();

    auto future = std::async(std::launch::async, [this, &station, from_vol, to_vol, duration_ms, captured_generation]() {
        const int steps = 50;
        const int step_delay_ms = duration_ms > 0 ? duration_ms / steps : 0;
        const double vol_step = (steps > 0) ? (to_vol - from_vol) / steps : (to_vol - from_vol);
        double current_vol = from_vol;
        
        for (int i = 0; i < steps; ++i) {
            if (!isFadeStillValid(station, captured_generation, to_vol)) return;

            current_vol += vol_step;
            station.setCurrentVolume(current_vol);
            double clamped_vol = std::max(0.0, std::min(100.0, current_vol));
            if (station.getMpvHandle()) {
                mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &clamped_vol);
            }
            m_app_state.needs_redraw = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(step_delay_ms));
        }

        if (isFadeStillValid(station, captured_generation, to_vol)) {
            station.setCurrentVolume(to_vol);
            double final_vol = std::max(0.0, std::min(100.0, to_vol));
            if (station.getMpvHandle()) {
               mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &final_vol);
            }
        }
        
        if (station.getGeneration() == captured_generation) {
             station.setFading(false);
        }
        m_app_state.needs_redraw = true;
    });
    std::lock_guard<std::mutex> lock(m_fade_futures_mutex);
    m_fade_futures.push_back(std::move(future));
}

void StationManager::cleanupFinishedFutures() {
    std::lock_guard<std::mutex> lock(m_fade_futures_mutex);
    m_fade_futures.erase(std::remove_if(m_fade_futures.begin(), m_fade_futures.end(), [](const std::future<void>& f) { return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }), m_fade_futures.end());
}
