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
    constexpr int CROSSFADE_TIME_MS = 1200;
    constexpr double DUCK_VOLUME = 40.0;
    constexpr auto ACTOR_LOOP_TIMEOUT = std::chrono::milliseconds(20);
    constexpr int FORGOTTEN_MUTE_SECONDS = 600;
    constexpr size_t MAX_NAV_HISTORY = 10;
    constexpr int HISTORY_WRITE_THRESHOLD = 5;
    constexpr int COPY_MODE_TIMEOUT_SECONDS = 10;
    constexpr int FOCUS_MODE_SECONDS = 90;
    constexpr int AUTO_HOP_TOTAL_TIME_SECONDS = 1125;
    constexpr int PENDING_INSTANCE_ID_OFFSET = 10000;
    constexpr int CYCLE_TIMEOUT_SECONDS = 8;
}

StationManager::StationManager(const StationData& station_data)
    : m_unsaved_history_count(0), m_active_station_idx(0),
      m_copy_mode_active(false), m_active_panel(ActivePanel::STATIONS),
      m_history_scroll_offset(0), m_auto_hop_mode_active(false),
      m_hopper_mode(HopperMode::BALANCED),
      m_last_switch_time(std::chrono::steady_clock::now()),
      m_session_start_time(std::chrono::steady_clock::now()),
      m_session_switches(0), m_new_songs_found(0), m_songs_copied(0),
      m_was_quit_by_mute_timeout(false),
      m_quit_flag(false), m_needs_redraw(true)
{
    if (station_data.empty()) {
        throw std::runtime_error("No radio stations provided.");
    }
    
    for (size_t i = 0; i < station_data.size(); ++i) {
        m_stations.emplace_back(i, station_data[i].first, station_data[i].second);
    }
    
    PersistenceManager persistence;
    m_song_history = std::make_unique<nlohmann::json>(persistence.loadHistory());
    if (!m_song_history->is_object()) {
        *m_song_history = nlohmann::json::object();
    }
    
    const auto favorite_names = persistence.loadFavoriteNames();
    for (auto& station : m_stations) {
        if (favorite_names.count(station.getName())) {
            station.toggleFavorite();
        }
        if (!m_song_history->contains(station.getName())) {
            (*m_song_history)[station.getName()] = nlohmann::json::array();
        }
    }

    if (auto last_station_name = persistence.loadLastStationName()) {
        auto it = std::find_if(m_stations.begin(), m_stations.end(),
                               [&](const RadioStream& station) { return station.getName() == *last_station_name; });
        if (it != m_stations.end()) {
            m_active_station_idx = std::distance(m_stations.begin(), it);
        }
    }

    m_event_handler = std::make_unique<MpvEventHandler>(*this);
    m_actor_thread = std::thread(&StationManager::actorLoop, this);
}

StationManager::~StationManager() {
    post(Msg::Quit{});
    if (m_actor_thread.joinable()) {
        m_actor_thread.join();
    }
    PersistenceManager persistence;
    persistence.saveHistory(*m_song_history);
    persistence.saveFavorites(m_stations);
    if (!m_stations.empty() && m_active_station_idx >= 0 && m_active_station_idx < (int)m_stations.size()) {
        persistence.saveSession(m_stations[m_active_station_idx].getName());
    }
    if (m_was_quit_by_mute_timeout) {
        std::cout << "Hey, you forgot about me for " << (FORGOTTEN_MUTE_SECONDS / 60) << " minutes! 😤" << std::endl;
    } else {
        auto end_time = std::chrono::steady_clock::now();
        auto duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_time - m_session_start_time).count();
        long duration_minutes = duration_seconds / 60;
        std::cout << "---\n" << "Thank you for using Stream Hopper!\n" << "🎛️ Session Switches: " << m_session_switches << "\n" << "✨ New Songs Found: " << m_new_songs_found << "\n" << "📋 Songs Copied: " << m_songs_copied << "\n" << "🕐 Total Time: " << duration_minutes << " minutes\n" << "---" << std::endl;
    }
}

StateSnapshot StationManager::createSnapshot() const {
    std::lock_guard<std::mutex> lock(m_stations_mutex);
    StateSnapshot snapshot;
    snapshot.active_station_idx = m_active_station_idx;
    snapshot.active_panel = m_active_panel;
    snapshot.is_copy_mode_active = m_copy_mode_active;
    snapshot.is_auto_hop_mode_active = m_auto_hop_mode_active;
    snapshot.history_scroll_offset = m_history_scroll_offset;
    snapshot.hopper_mode = m_hopper_mode;
    snapshot.stations.reserve(m_stations.size());
    for (const auto& station : m_stations) {
        snapshot.stations.push_back({
            .name = station.getName(), .current_title = station.getCurrentTitle(),
            .bitrate = station.getBitrate(), .current_volume = station.getCurrentVolume(),
            .is_initialized = station.isInitialized(), .is_favorite = station.isFavorite(),
            .is_buffering = station.isBuffering(), .playback_state = station.getPlaybackState(),
            .cycling_state = station.getCyclingState(), .pending_bitrate = station.getPendingBitrate(),
            .url_count = station.getAllUrls().size()
        });
    }
    snapshot.current_volume_for_header = 0.0;
    if (!snapshot.stations.empty()) {
        const auto& active_station_data = snapshot.stations[snapshot.active_station_idx];
        if (active_station_data.is_initialized) {
            snapshot.current_volume_for_header = active_station_data.playback_state == PlaybackState::Muted ? 0.0 : active_station_data.current_volume;
        }
        const auto& active_station_name = active_station_data.name;
        if(m_song_history->contains(active_station_name)) {
            snapshot.active_station_history = (*m_song_history)[active_station_name];
        } else {
            snapshot.active_station_history = nlohmann::json::array();
        }
    }
    snapshot.auto_hop_total_duration = 0;
    if (!m_stations.empty()) {
        snapshot.auto_hop_total_duration = AUTO_HOP_TOTAL_TIME_SECONDS / static_cast<int>(m_stations.size());
    }
    snapshot.auto_hop_remaining_seconds = 0;
    if (m_auto_hop_mode_active) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_s = std::chrono::duration_cast<std::chrono::seconds>(now - m_auto_hop_start_time).count();
        snapshot.auto_hop_remaining_seconds = std::max(0, snapshot.auto_hop_total_duration - static_cast<int>(elapsed_s));
    }
    return snapshot;
}

void StationManager::post(StationManagerMessage message) {
    { std::lock_guard<std::mutex> lock(m_queue_mutex); m_message_queue.push_back(std::move(message)); }
    m_queue_cond.notify_one();
}

std::atomic<bool>& StationManager::getQuitFlag() { return m_quit_flag; }
std::atomic<bool>& StationManager::getNeedsRedrawFlag() { return m_needs_redraw; }

void StationManager::actorLoop() {
    updateActiveWindow();
    while(!m_quit_flag) {
        std::deque<StationManagerMessage> current_queue;
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            if (m_queue_cond.wait_for(lock, ACTOR_LOOP_TIMEOUT, [this] { return !m_message_queue.empty() || m_quit_flag; })) {
                current_queue.swap(m_message_queue);
            }
        }
        if (m_quit_flag) break;
        std::lock_guard<std::mutex> lock(m_stations_mutex);
        if (current_queue.empty()) {
            current_queue.push_back(Msg::UpdateAndPoll{});
        }
        for (auto& msg : current_queue) {
            std::visit([this](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if      constexpr (std::is_same_v<T, Msg::NavigateUp>)       handle_navigate(NavDirection::UP);
                else if constexpr (std::is_same_v<T, Msg::NavigateDown>)     handle_navigate(NavDirection::DOWN);
                else if constexpr (std::is_same_v<T, Msg::ToggleMute>)       handle_toggleMute();
                else if constexpr (std::is_same_v<T, Msg::ToggleAutoHop>)    handle_toggleAutoHop();
                else if constexpr (std::is_same_v<T, Msg::ToggleFavorite>)   handle_toggleFavorite();
                else if constexpr (std::is_same_v<T, Msg::ToggleDucking>)    handle_toggleDucking();
                else if constexpr (std::is_same_v<T, Msg::ToggleCopyMode>)   handle_toggleCopyMode();
                else if constexpr (std::is_same_v<T, Msg::ToggleHopperMode>) handle_toggleHopperMode();
                else if constexpr (std::is_same_v<T, Msg::SwitchPanel>)      handle_switchPanel();
                else if constexpr (std::is_same_v<T, Msg::CycleUrl>)         handle_cycleUrl();
                else if constexpr (std::is_same_v<T, Msg::UpdateAndPoll>)    handle_updateAndPoll();
                else if constexpr (std::is_same_v<T, Msg::Quit>)            handle_quit();
            }, msg);
             if (m_quit_flag) break;
        }
    }
    for (int station_idx : m_active_station_indices) {
        if(station_idx >=0 && station_idx < (int)m_stations.size()) {
            m_stations[station_idx].shutdown();
        }
    }
    m_active_station_indices.clear();
    m_active_fades.clear();
}

void StationManager::handle_navigate(NavDirection direction) {
    if (m_active_station_idx >= 0 && m_active_station_idx < (int)m_stations.size()) {
        auto& current_station_obj = m_stations[m_active_station_idx];
        if (current_station_obj.getCyclingState() != CyclingState::IDLE) {
            current_station_obj.finalizeCycle(false);
        }
    }

    if (m_hopper_mode == HopperMode::FOCUS) m_hopper_mode = HopperMode::BALANCED;
    if (m_active_panel == ActivePanel::STATIONS) {
        if (m_stations.empty()) return;
        int station_count = m_stations.size();
        int old_idx = m_active_station_idx;
        int new_idx = (direction == NavDirection::DOWN) ? (old_idx + 1) % station_count : (old_idx - 1 + station_count) % station_count;
        if (new_idx != old_idx) {
            RadioStream& current_station = m_stations[old_idx];
            if (current_station.isInitialized() && current_station.getPlaybackState() != PlaybackState::Muted) {
                fadeAudio(old_idx, 0.0, FADE_TIME_MS, false);
            }
            m_session_switches++;
            m_last_switch_time = std::chrono::steady_clock::now();
        }
        m_active_station_idx = new_idx;
        m_nav_history.push_back({direction, std::chrono::steady_clock::now()});
        if (m_nav_history.size() > MAX_NAV_HISTORY) {
            m_nav_history.pop_front();
        }
        updateActiveWindow();
        m_history_scroll_offset = 0;
    } else {
        size_t history_size = 0;
        if (!m_stations.empty()) {
            const auto& name = m_stations[m_active_station_idx].getName();
            if (m_song_history->contains(name)) {
                history_size = (*m_song_history)[name].size();
            }
        }
        if (direction == NavDirection::UP) {
            if (m_history_scroll_offset > 0) m_history_scroll_offset--;
        } else {
            if (history_size > 0 && m_history_scroll_offset < (int)history_size - 1) m_history_scroll_offset++;
        }
    }
    m_needs_redraw = true;
}

void StationManager::handle_cycleUrl() {
    if (m_active_station_idx < 0 || m_active_station_idx >= (int)m_stations.size()) return;
    RadioStream& station = m_stations[m_active_station_idx];
    if (station.getCyclingState() != CyclingState::IDLE || station.getAllUrls().size() <= 1) return;

    station.startCycle();
    m_needs_redraw = true;

    MpvInstance& pending_instance = station.getPendingMpvInstance();
    pending_instance.initialize(station.getNextUrl());

    double vol = 0.0;
    mpv_set_property(pending_instance.get(), "volume", MPV_FORMAT_DOUBLE, &vol);
    const char* cmd[] = {"loadfile", station.getNextUrl().c_str(), "replace", nullptr};
    check_mpv_error(mpv_command_async(pending_instance.get(), 0, cmd), "loadfile for pending cycle");

    const int reply_id = station.getID() + PENDING_INSTANCE_ID_OFFSET;
    check_mpv_error(mpv_observe_property(pending_instance.get(), reply_id, "media-title", MPV_FORMAT_STRING), "observe pending media-title");
    check_mpv_error(mpv_observe_property(pending_instance.get(), reply_id, "audio-bitrate", MPV_FORMAT_INT64), "observe pending audio-bitrate");
}

void StationManager::handle_cycle_status_timers() {
    for(auto& station : m_stations) {
        if(station.getCyclingState() == CyclingState::SUCCEEDED || station.getCyclingState() == CyclingState::FAILED) {
            if(std::chrono::steady_clock::now() >= station.getCycleStatusEndTime()) {
                station.clearCycleStatus();
                m_needs_redraw = true;
            }
        }
    }
}

void StationManager::handle_cycle_timeouts() {
    auto now = std::chrono::steady_clock::now();
    for (auto& station : m_stations) {
        if (station.getCyclingState() == CyclingState::CYCLING) {
            if (auto start_time = station.getCycleStartTime()) {
                if (std::chrono::duration_cast<std::chrono::seconds>(now - *start_time).count() >= CYCLE_TIMEOUT_SECONDS) {
                    station.finalizeCycle(false);
                    m_needs_redraw = true;
                }
            }
        }
    }
}

void StationManager::crossFadeToPending(int station_id) {
    if (station_id < 0 || station_id >= (int)m_stations.size()) return;
    fadeAudio(station_id, 0.0, CROSSFADE_TIME_MS, false);
    fadeAudio(station_id, 100.0, CROSSFADE_TIME_MS, true);
}

void StationManager::handle_updateAndPoll() {
    handle_cycle_status_timers();
    handle_cycle_timeouts();
    handle_activeFades();
    pollMpvEvents();

    auto now = std::chrono::steady_clock::now();
    if (m_copy_mode_active) {
        if (std::chrono::duration_cast<std::chrono::seconds>(now - m_copy_mode_start_time).count() >= COPY_MODE_TIMEOUT_SECONDS) {
            handle_toggleCopyMode();
        }
    }
    if (m_auto_hop_mode_active) {
        auto station_count = m_stations.size();
        if (station_count > 0) {
            int duration = AUTO_HOP_TOTAL_TIME_SECONDS / static_cast<int>(station_count);
            if (std::chrono::duration_cast<std::chrono::seconds>(now - m_auto_hop_start_time).count() >= duration) {
                handle_navigate(NavDirection::DOWN);
                m_auto_hop_start_time = std::chrono::steady_clock::now();
            }
        }
    }
    if (!m_auto_hop_mode_active && m_hopper_mode != HopperMode::FOCUS) {
        if (std::chrono::duration_cast<std::chrono::seconds>(now - m_last_switch_time).count() >= FOCUS_MODE_SECONDS) {
            m_hopper_mode = HopperMode::FOCUS;
            updateActiveWindow();
            m_needs_redraw = true;
        }
    }
    if (!m_auto_hop_mode_active && !m_stations.empty()) {
        const auto& active_station = m_stations[m_active_station_idx];
        if (active_station.getPlaybackState() == PlaybackState::Muted) {
            if (auto mute_start = active_station.getMuteStartTime()) {
                if (std::chrono::duration_cast<std::chrono::seconds>(now - *mute_start).count() >= FORGOTTEN_MUTE_SECONDS) {
                    m_was_quit_by_mute_timeout = true;
                    handle_quit();
                }
            }
        }
    }
}

void StationManager::pollMpvEvents() {
    bool events_pending = true;
    while(events_pending) {
        events_pending = false;
        
        const auto indices_to_poll = m_active_station_indices;
        for(int station_idx : indices_to_poll) { 
            if (station_idx >= (int)m_stations.size() || !m_stations[station_idx].isInitialized()) continue;
            mpv_event *event = mpv_wait_event(m_stations[station_idx].getMpvHandle(), 0);
            if (event->event_id != MPV_EVENT_NONE) {
                m_event_handler->handleEvent(event);
                events_pending = true;
            }
        }

        if(m_active_station_idx >=0 && m_active_station_idx < (int)m_stations.size()) {
            auto& station = m_stations[m_active_station_idx];
            if(station.getCyclingState() == CyclingState::CYCLING && station.getPendingMpvInstance().get()) {
                mpv_event* event = mpv_wait_event(station.getPendingMpvInstance().get(), 0);
                if (event->event_id != MPV_EVENT_NONE) {
                    m_event_handler->handleEvent(event);
                    events_pending = true;
                }
            }
        }
    }
}

void StationManager::fadeAudio(int station_id, double to_vol, int duration_ms, bool for_pending) {
    if (station_id < 0 || station_id >= (int)m_stations.size()) return;
    
    m_active_fades.erase(std::remove_if(m_active_fades.begin(), m_active_fades.end(),
        [station_id, for_pending](const ActiveFade& f){
            return f.station_id == station_id && f.is_for_pending_instance == for_pending;
        }), m_active_fades.end());
    
    RadioStream& station = m_stations[station_id];
    mpv_handle* handle = for_pending ? station.getPendingMpvInstance().get() : station.getMpvHandle();
    if (!handle) return;
    
    m_active_fades.push_back({
        .station_id = station_id, .generation = station.getGeneration(),
        .start_vol = for_pending ? 0.0 : station.getCurrentVolume(),
        .target_vol = to_vol, .start_time = std::chrono::steady_clock::now(),
        .duration_ms = duration_ms, .is_for_pending_instance = for_pending
    });
}

void StationManager::handle_activeFades() {
    if (m_active_fades.empty()) return;
    auto now = std::chrono::steady_clock::now();
    bool changed = false;

    m_active_fades.erase(std::remove_if(m_active_fades.begin(), m_active_fades.end(),
        [&](ActiveFade& fade) -> bool {
            if (fade.station_id < 0 || fade.station_id >= (int)m_stations.size()) {
                return true;
            }
            RadioStream& station = m_stations[fade.station_id];
            
            if (station.getGeneration() != fade.generation) {
                return true;
            }

            mpv_handle* handle = fade.is_for_pending_instance ? station.getPendingMpvInstance().get() : station.getMpvHandle();
            if (!handle) {
                return true;
            }

            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - fade.start_time).count();
            double progress = (fade.duration_ms > 0) ? std::min(1.0, static_cast<double>(elapsed_ms) / fade.duration_ms) : 1.0;
            double new_vol = fade.start_vol + (fade.target_vol - fade.start_vol) * progress;
            
            if (!fade.is_for_pending_instance) {
                station.setCurrentVolume(new_vol);
            }
            
            double clamped_vol = std::max(0.0, std::min(100.0, new_vol));
            mpv_set_property_async(handle, 0, "volume", MPV_FORMAT_DOUBLE, &clamped_vol);
            changed = true;

            if (progress >= 1.0) {
                if (fade.is_for_pending_instance) {
                    station.promotePendingMetadata();
                    station.promotePendingToActive();
                    station.setCurrentVolume(fade.target_vol);
                    station.finalizeCycle(true);
                } else if (station.getCyclingState() == CyclingState::SUCCEEDED) {
                    station.getPendingMpvInstance().shutdown();
                }
                return true;
            }
            return false;
        }),
    m_active_fades.end());

    if (changed) m_needs_redraw = true;
}

void StationManager::handle_toggleMute() {
    if (m_active_station_idx < 0 || m_active_station_idx >= (int)m_stations.size()) return;
    RadioStream& station = m_stations[m_active_station_idx];
    if (!station.isInitialized() || station.getPlaybackState() == PlaybackState::Ducked) return;
    if (station.getPlaybackState() == PlaybackState::Muted) {
        station.setPlaybackState(PlaybackState::Playing);
        station.resetMuteStartTime();
        fadeAudio(m_active_station_idx, station.getPreMuteVolume(), FADE_TIME_MS / 2, false);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setPlaybackState(PlaybackState::Muted);
        station.setMuteStartTime();
        fadeAudio(m_active_station_idx, 0.0, FADE_TIME_MS / 2, false);
    }
    m_needs_redraw = true;
}

void StationManager::handle_toggleAutoHop() {
    m_auto_hop_mode_active = !m_auto_hop_mode_active;
    if (m_auto_hop_mode_active) {
        m_last_switch_time = std::chrono::steady_clock::now();
        m_auto_hop_start_time = std::chrono::steady_clock::now();
        if(!m_stations.empty()) {
            const auto& station = m_stations[m_active_station_idx];
            if (station.getPlaybackState() != PlaybackState::Playing) {
                handle_toggleMute();
            }
            if (station.getCurrentVolume() < 50.0) {
                fadeAudio(m_active_station_idx, 100.0, FADE_TIME_MS, false);
            }
        }
    }
    m_needs_redraw = true;
}

void StationManager::handle_toggleFavorite() {
    if (m_active_station_idx >= 0 && m_active_station_idx < (int)m_stations.size()) {
        m_stations[m_active_station_idx].toggleFavorite();
    }
    m_needs_redraw = true;
}

void StationManager::handle_toggleDucking() {
    if (m_active_station_idx < 0 || m_active_station_idx >= (int)m_stations.size()) return;
    RadioStream& station = m_stations[m_active_station_idx];
    if (!station.isInitialized() || station.getPlaybackState() == PlaybackState::Muted) return;
    if (station.getPlaybackState() == PlaybackState::Ducked) {
        station.setPlaybackState(PlaybackState::Playing);
        fadeAudio(m_active_station_idx, station.getPreMuteVolume(), FADE_TIME_MS, false);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setPlaybackState(PlaybackState::Ducked);
        fadeAudio(m_active_station_idx, DUCK_VOLUME, FADE_TIME_MS, false);
    }
    m_needs_redraw = true;
}

void StationManager::handle_toggleCopyMode() {
    m_copy_mode_active = !m_copy_mode_active;
    if (m_copy_mode_active) {
        m_copy_mode_start_time = std::chrono::steady_clock::now();
    }
    m_needs_redraw = true;
}

void StationManager::handle_toggleHopperMode() {
    m_hopper_mode = (m_hopper_mode == HopperMode::PERFORMANCE) ? HopperMode::BALANCED : HopperMode::PERFORMANCE;
    updateActiveWindow();
    m_needs_redraw = true;
}

void StationManager::handle_switchPanel() {
    m_active_panel = (m_active_panel == ActivePanel::STATIONS) ? ActivePanel::HISTORY : ActivePanel::STATIONS;
    m_needs_redraw = true;
}

void StationManager::handle_quit() {
    m_quit_flag = true;
}

void StationManager::updateActiveWindow() {
    const auto new_active_set = m_preloader.calculate_active_indices( m_active_station_idx, m_stations.size(), m_hopper_mode, m_nav_history);
    std::vector<int> to_shutdown;
    for (int idx : m_active_station_indices) {
        if (new_active_set.find(idx) == new_active_set.end()) {
            to_shutdown.push_back(idx);
        }
    }
    for (int idx : to_shutdown) {
        shutdownStation(idx);
    }
    for (int idx : new_active_set) {
        if (m_active_station_indices.find(idx) == m_active_station_indices.end()) {
            initializeStation(idx);
        }
    }
    if(m_active_station_idx >= 0 && m_active_station_idx < (int)m_stations.size()) {
        RadioStream& new_station = m_stations[m_active_station_idx];
        if (new_station.isInitialized() && new_station.getPlaybackState() != PlaybackState::Muted && new_station.getCurrentVolume() < 99.0) {
            fadeAudio(m_active_station_idx, 100.0, FADE_TIME_MS, false);
        }
    }
}

void StationManager::initializeStation(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    double vol = (station_idx == m_active_station_idx) ? 100.0 : 0.0;
    m_stations[station_idx].initialize(vol);
    m_active_station_indices.insert(station_idx);
}

void StationManager::shutdownStation(int station_idx) {
    if (station_idx < 0 || station_idx >= (int)m_stations.size()) return;
    m_stations[station_idx].shutdown();
    m_active_station_indices.erase(station_idx);
}

void StationManager::saveHistoryToDisk() {
    PersistenceManager persistence;
    persistence.saveHistory(*m_song_history);
    m_unsaved_history_count = 0;
}

void StationManager::addHistoryEntry(const std::string& station_name, const nlohmann::json& entry) {
    (*m_song_history)[station_name].push_back(entry);
    m_new_songs_found++;
    if (++m_unsaved_history_count >= HISTORY_WRITE_THRESHOLD) {
        saveHistoryToDisk();
    }
}
