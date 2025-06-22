#include "Core/MessageHandler.h"
#include "StationManager.h" // Include the full definition here
#include "RadioStream.h"
#include "SessionState.h"
#include "Utils.h" // <-- ADD THIS INCLUDE
#include <chrono>

// Constants can be moved here if they are only used by handlers
namespace {
    constexpr int FADE_TIME_MS = 900;
    constexpr double DUCK_VOLUME = 40.0;
    constexpr int COPY_MODE_TIMEOUT_SECONDS = 10;
    constexpr int FOCUS_MODE_SECONDS = 90;
    constexpr int AUTO_HOP_TOTAL_TIME_SECONDS = 1125;
    constexpr int FORGOTTEN_MUTE_SECONDS = 600;
    constexpr int PENDING_INSTANCE_ID_OFFSET = 10000;
    constexpr int CYCLE_TIMEOUT_SECONDS = 8;
}

void MessageHandler::handle_navigate(StationManager& manager, NavDirection direction) {
    if (manager.m_session_state.active_station_idx >= 0 && manager.m_session_state.active_station_idx < (int)manager.m_stations.size()) {
        auto& current_station_obj = manager.m_stations[manager.m_session_state.active_station_idx];
        if (current_station_obj.getCyclingState() != CyclingState::IDLE) {
            current_station_obj.finalizeCycle(false);
        }
    }

    if (manager.m_session_state.hopper_mode == HopperMode::FOCUS) manager.m_session_state.hopper_mode = HopperMode::BALANCED;
    if (manager.m_session_state.active_panel == ActivePanel::STATIONS) {
        if (manager.m_stations.empty()) return;
        int station_count = manager.m_stations.size();
        int old_idx = manager.m_session_state.active_station_idx;
        int new_idx = (direction == NavDirection::DOWN) ? (old_idx + 1) % station_count : (old_idx - 1 + station_count) % station_count;
        if (new_idx != old_idx) {
            RadioStream& current_station = manager.m_stations[old_idx];
            if (current_station.isInitialized() && current_station.getPlaybackState() != PlaybackState::Muted) {
                manager.fadeAudio(old_idx, 0.0, FADE_TIME_MS, false);
            }
            manager.m_session_state.session_switches++;
            manager.m_session_state.last_switch_time = std::chrono::steady_clock::now();
        }
        manager.m_session_state.active_station_idx = new_idx;
        manager.m_session_state.nav_history.push_back({direction, std::chrono::steady_clock::now()});
        if (manager.m_session_state.nav_history.size() > manager.MAX_NAV_HISTORY) {
            manager.m_session_state.nav_history.pop_front();
        }
        manager.updateActiveWindow();
        manager.m_session_state.history_scroll_offset = 0;
    } else {
        size_t history_size = 0;
        if (!manager.m_stations.empty()) {
            const auto& name = manager.m_stations[manager.m_session_state.active_station_idx].getName();
            if (manager.m_song_history->contains(name)) {
                history_size = (*manager.m_song_history)[name].size();
            }
        }
        if (direction == NavDirection::UP) {
            if (manager.m_session_state.history_scroll_offset > 0) manager.m_session_state.history_scroll_offset--;
        } else {
            if (history_size > 0 && manager.m_session_state.history_scroll_offset < (int)history_size - 1) manager.m_session_state.history_scroll_offset++;
        }
    }
    manager.m_needs_redraw = true;
}

void MessageHandler::handle_cycleUrl(StationManager& manager) {
    if (manager.m_session_state.active_station_idx < 0 || manager.m_session_state.active_station_idx >= (int)manager.m_stations.size()) return;
    RadioStream& station = manager.m_stations[manager.m_session_state.active_station_idx];
    if (station.getCyclingState() != CyclingState::IDLE || station.getAllUrls().size() <= 1) return;

    station.startCycle();
    manager.m_needs_redraw = true;

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

void MessageHandler::handle_updateAndPoll(StationManager& manager) {
    manager.handle_cycle_status_timers();
    manager.handle_cycle_timeouts();
    manager.handle_activeFades();
    manager.pollMpvEvents();

    auto now = std::chrono::steady_clock::now();
    if (manager.m_session_state.copy_mode_active) {
        if (std::chrono::duration_cast<std::chrono::seconds>(now - manager.m_session_state.copy_mode_start_time).count() >= COPY_MODE_TIMEOUT_SECONDS) {
            handle_toggleCopyMode(manager);
        }
    }
    if (manager.m_session_state.auto_hop_mode_active) {
        auto station_count = manager.m_stations.size();
        if (station_count > 0) {
            int duration = AUTO_HOP_TOTAL_TIME_SECONDS / static_cast<int>(station_count);
            if (std::chrono::duration_cast<std::chrono::seconds>(now - manager.m_session_state.auto_hop_start_time).count() >= duration) {
                handle_navigate(manager, NavDirection::DOWN);
                manager.m_session_state.auto_hop_start_time = std::chrono::steady_clock::now();
            }
        }
    }
    if (!manager.m_session_state.auto_hop_mode_active && manager.m_session_state.hopper_mode != HopperMode::FOCUS) {
        if (std::chrono::duration_cast<std::chrono::seconds>(now - manager.m_session_state.last_switch_time).count() >= FOCUS_MODE_SECONDS) {
            manager.m_session_state.hopper_mode = HopperMode::FOCUS;
            manager.updateActiveWindow();
            manager.m_needs_redraw = true;
        }
    }
    if (!manager.m_session_state.auto_hop_mode_active && !manager.m_stations.empty()) {
        const auto& active_station = manager.m_stations[manager.m_session_state.active_station_idx];
        if (active_station.getPlaybackState() == PlaybackState::Muted) {
            if (auto mute_start = active_station.getMuteStartTime()) {
                if (std::chrono::duration_cast<std::chrono::seconds>(now - *mute_start).count() >= FORGOTTEN_MUTE_SECONDS) {
                    manager.m_session_state.was_quit_by_mute_timeout = true;
                    handle_quit(manager);
                }
            }
        }
    }

    // Check for active animations that require a constant redraw heartbeat.
    bool is_any_station_cycling = std::any_of(manager.m_stations.begin(), manager.m_stations.end(),
        [](const RadioStream& s) { return s.getCyclingState() == CyclingState::CYCLING; });

    if (manager.m_session_state.auto_hop_mode_active || is_any_station_cycling) {
        manager.m_needs_redraw = true;
    }
}

void MessageHandler::handle_toggleMute(StationManager& manager) {
    if (manager.m_session_state.active_station_idx < 0 || manager.m_session_state.active_station_idx >= (int)manager.m_stations.size()) return;
    RadioStream& station = manager.m_stations[manager.m_session_state.active_station_idx];
    if (!station.isInitialized() || station.getPlaybackState() == PlaybackState::Ducked) return;
    if (station.getPlaybackState() == PlaybackState::Muted) {
        station.setPlaybackState(PlaybackState::Playing);
        station.resetMuteStartTime();
        manager.fadeAudio(manager.m_session_state.active_station_idx, station.getPreMuteVolume(), FADE_TIME_MS / 2, false);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setPlaybackState(PlaybackState::Muted);
        station.setMuteStartTime();
        manager.fadeAudio(manager.m_session_state.active_station_idx, 0.0, FADE_TIME_MS / 2, false);
    }
    manager.m_needs_redraw = true;
}

void MessageHandler::handle_toggleAutoHop(StationManager& manager) {
    manager.m_session_state.auto_hop_mode_active = !manager.m_session_state.auto_hop_mode_active;
    if (manager.m_session_state.auto_hop_mode_active) {
        manager.m_session_state.last_switch_time = std::chrono::steady_clock::now();
        manager.m_session_state.auto_hop_start_time = std::chrono::steady_clock::now();
        if(!manager.m_stations.empty()) {
            const auto& station = manager.m_stations[manager.m_session_state.active_station_idx];
            if (station.getPlaybackState() != PlaybackState::Playing) {
                handle_toggleMute(manager);
            }
            if (station.getCurrentVolume() < 50.0) {
                manager.fadeAudio(manager.m_session_state.active_station_idx, 100.0, FADE_TIME_MS, false);
            }
        }
    }
    manager.m_needs_redraw = true;
}

void MessageHandler::handle_toggleFavorite(StationManager& manager) {
    if (manager.m_session_state.active_station_idx >= 0 && manager.m_session_state.active_station_idx < (int)manager.m_stations.size()) {
        manager.m_stations[manager.m_session_state.active_station_idx].toggleFavorite();
    }
    manager.m_needs_redraw = true;
}

void MessageHandler::handle_toggleDucking(StationManager& manager) {
    if (manager.m_session_state.active_station_idx < 0 || manager.m_session_state.active_station_idx >= (int)manager.m_stations.size()) return;
    RadioStream& station = manager.m_stations[manager.m_session_state.active_station_idx];
    if (!station.isInitialized() || station.getPlaybackState() == PlaybackState::Muted) return;
    if (station.getPlaybackState() == PlaybackState::Ducked) {
        station.setPlaybackState(PlaybackState::Playing);
        manager.fadeAudio(manager.m_session_state.active_station_idx, station.getPreMuteVolume(), FADE_TIME_MS, false);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setPlaybackState(PlaybackState::Ducked);
        manager.fadeAudio(manager.m_session_state.active_station_idx, DUCK_VOLUME, FADE_TIME_MS, false);
    }
    manager.m_needs_redraw = true;
}

void MessageHandler::handle_toggleCopyMode(StationManager& manager) {
    manager.m_session_state.copy_mode_active = !manager.m_session_state.copy_mode_active;
    if (manager.m_session_state.copy_mode_active) {
        manager.m_session_state.copy_mode_start_time = std::chrono::steady_clock::now();
    }
    manager.m_needs_redraw = true;
}

void MessageHandler::handle_toggleHopperMode(StationManager& manager) {
    manager.m_session_state.hopper_mode = (manager.m_session_state.hopper_mode == HopperMode::PERFORMANCE) ? HopperMode::BALANCED : HopperMode::PERFORMANCE;
    manager.updateActiveWindow();
    manager.m_needs_redraw = true;
}

void MessageHandler::handle_switchPanel(StationManager& manager) {
    manager.m_session_state.active_panel = (manager.m_session_state.active_panel == ActivePanel::STATIONS) ? ActivePanel::HISTORY : ActivePanel::STATIONS;
    manager.m_needs_redraw = true;
}

void MessageHandler::handle_quit(StationManager& manager) {
    manager.m_quit_flag = true;
}
