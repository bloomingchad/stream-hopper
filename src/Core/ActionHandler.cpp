#include "Core/ActionHandler.h"

#include <chrono>
#include <utility> // For std::move
#include <variant>

#include "Core/VolumeNormalizer.h"
#include "RadioStream.h"
#include "SessionState.h"
#include "StationManager.h"
#include "Utils.h"

namespace {
    constexpr int FADE_TIME_MS = 900;
    constexpr double DUCK_VOLUME = 40.0;
    constexpr int PENDING_INSTANCE_ID_OFFSET = 10000;
    constexpr double VOLUME_ADJUST_AMOUNT = 1.0;
}

void ActionHandler::process_action(StationManager& manager, const StationManagerMessage& msg) {
    std::visit(
        [this, &manager](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Msg::NavigateUp>)
                handle_navigate(manager, NavDirection::UP);
            else if constexpr (std::is_same_v<T, Msg::NavigateDown>)
                handle_navigate(manager, NavDirection::DOWN);
            else if constexpr (std::is_same_v<T, Msg::ToggleMute>)
                handle_toggleMute(manager);
            else if constexpr (std::is_same_v<T, Msg::ToggleAutoHop>)
                handle_toggleAutoHop(manager);
            else if constexpr (std::is_same_v<T, Msg::ToggleFavorite>)
                handle_toggleFavorite(manager);
            else if constexpr (std::is_same_v<T, Msg::ToggleDucking>)
                handle_toggleDucking(manager);
            else if constexpr (std::is_same_v<T, Msg::ToggleCopyMode>)
                handle_toggleCopyMode(manager);
            else if constexpr (std::is_same_v<T, Msg::ToggleHopperMode>)
                handle_toggleHopperMode(manager);
            else if constexpr (std::is_same_v<T, Msg::SwitchPanel>)
                handle_switchPanel(manager);
            else if constexpr (std::is_same_v<T, Msg::CycleUrl>)
                handle_cycleUrl(manager);
            else if constexpr (std::is_same_v<T, Msg::SearchOnline>)
                handle_searchOnline(manager, arg.key);
            else if constexpr (std::is_same_v<T, Msg::AdjustVolumeOffsetUp>)
                handle_adjustVolumeOffset(manager, VOLUME_ADJUST_AMOUNT);
            else if constexpr (std::is_same_v<T, Msg::AdjustVolumeOffsetDown>)
                handle_adjustVolumeOffset(manager, -VOLUME_ADJUST_AMOUNT);
        },
        msg);
}

void ActionHandler::handle_adjustVolumeOffset(StationManager& manager, double amount) {
    if (manager.m_session_state.active_station_idx < 0 ||
        manager.m_session_state.active_station_idx >= (int) manager.m_stations.size()) {
        return;
    }
    if (manager.m_session_state.active_panel == ActivePanel::HISTORY) {
        return; // Don't adjust volume when scrolling history
    }

    RadioStream& station = manager.m_stations[manager.m_session_state.active_station_idx];
    if (!station.isInitialized()) {
        return;
    }
    manager.m_volume_normalizer->adjust(manager, station, amount);
    manager.m_needs_redraw = true;
}

void ActionHandler::handle_searchOnline(StationManager& manager, char key) {
    if (manager.m_search_providers.find(key) == manager.m_search_providers.end()) {
        return; // Key not found in config
    }
    const auto& provider = manager.m_search_providers.at(key);

    if (manager.m_stations.empty() || manager.m_session_state.active_station_idx < 0)
        return;

    const auto& station = manager.m_stations[manager.m_session_state.active_station_idx];
    std::string title = station.getCurrentTitle();

    if (title.empty() || title == "..." || title == "Initializing..." || title == "Buffering..." ||
        title.find("Stream Error") != std::string::npos) {
        return;
    }

    std::string full_url = provider.base_url + url_encode(title, provider.encoding_style);
    std::string error_message;
    if (!execute_open_command(full_url, error_message)) {
        manager.m_session_state.temporary_status_message = std::move(error_message);
        manager.m_session_state.temporary_message_end_time = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        manager.m_needs_redraw = true;
    } else {
        manager.m_session_state.songs_copied++;
    }
}

void ActionHandler::handle_navigateStations(StationManager& manager, NavDirection direction) {
    if (manager.m_session_state.active_station_idx >= 0 &&
        manager.m_session_state.active_station_idx < (int) manager.m_stations.size()) {
        auto& current_station_obj = manager.m_stations[manager.m_session_state.active_station_idx];
        if (current_station_obj.getCyclingState() != CyclingState::IDLE) {
            current_station_obj.finalizeCycle(false);
        }
    }

    if (manager.m_stations.empty())
        return;
    int station_count = manager.m_stations.size();
    int old_idx = manager.m_session_state.active_station_idx;
    int new_idx = (direction == NavDirection::DOWN) ? (old_idx + 1) % station_count
                                                    : (old_idx - 1 + station_count) % station_count;
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
}

void ActionHandler::handle_navigateHistory(StationManager& manager, NavDirection direction) {
    size_t history_size = 0;
    if (!manager.m_stations.empty()) {
        const auto& name = manager.m_stations[manager.m_session_state.active_station_idx].getName();
        if (manager.m_song_history->contains(name)) {
            history_size = (*manager.m_song_history)[name].size();
        }
    }
    if (direction == NavDirection::UP) {
        if (manager.m_session_state.history_scroll_offset > 0)
            manager.m_session_state.history_scroll_offset--;
    } else {
        if (history_size > 0 && manager.m_session_state.history_scroll_offset < (int) history_size - 1)
            manager.m_session_state.history_scroll_offset++;
    }
}

void ActionHandler::handle_navigate(StationManager& manager, NavDirection direction) {
    if (manager.m_session_state.hopper_mode == HopperMode::FOCUS)
        manager.m_session_state.hopper_mode = HopperMode::BALANCED;
    if (manager.m_session_state.active_panel == ActivePanel::STATIONS) {
        handle_navigateStations(manager, direction);
    } else {
        handle_navigateHistory(manager, direction);
    }
    manager.m_needs_redraw = true;
}

void ActionHandler::handle_cycleUrl(StationManager& manager) {
    if (manager.m_session_state.active_station_idx < 0 ||
        manager.m_session_state.active_station_idx >= (int) manager.m_stations.size())
        return;
    RadioStream& station = manager.m_stations[manager.m_session_state.active_station_idx];
    if (station.getCyclingState() != CyclingState::IDLE || station.getAllUrls().size() <= 1)
        return;

    station.startCycle();
    manager.m_needs_redraw = true;

    MpvInstance& pending_instance = station.getPendingMpvInstance();
    pending_instance.initialize(station.getNextUrl());
    manager.applyCombinedVolume(station.getID(), true); // Apply 0 volume to pending

    const char* cmd[] = {"loadfile", station.getNextUrl().c_str(), "replace", nullptr};
    check_mpv_error(mpv_command_async(pending_instance.get(), 0, cmd), "loadfile for pending cycle");

    const int reply_id = station.getID() + PENDING_INSTANCE_ID_OFFSET;
    check_mpv_error(mpv_observe_property(pending_instance.get(), reply_id, "media-title", MPV_FORMAT_STRING),
                    "observe pending media-title");
    check_mpv_error(mpv_observe_property(pending_instance.get(), reply_id, "audio-bitrate", MPV_FORMAT_INT64),
                    "observe pending audio-bitrate");
}

void ActionHandler::handle_toggleMute(StationManager& manager) {
    if (manager.m_session_state.active_station_idx < 0 ||
        manager.m_session_state.active_station_idx >= (int) manager.m_stations.size())
        return;
    RadioStream& station = manager.m_stations[manager.m_session_state.active_station_idx];
    if (!station.isInitialized() || station.getPlaybackState() == PlaybackState::Ducked)
        return;
    if (station.getPlaybackState() == PlaybackState::Muted) {
        station.setPlaybackState(PlaybackState::Playing);
        station.resetMuteStartTime();
        manager.fadeAudio(manager.m_session_state.active_station_idx, station.getPreMuteVolume(), FADE_TIME_MS / 2,
                          false);
    } else {
        station.setPreMuteVolume(station.getCurrentVolume());
        station.setPlaybackState(PlaybackState::Muted);
        station.setMuteStartTime();
        manager.fadeAudio(manager.m_session_state.active_station_idx, 0.0, FADE_TIME_MS / 2, false);
    }
    manager.m_needs_redraw = true;
}

void ActionHandler::handle_toggleAutoHop(StationManager& manager) {
    manager.m_session_state.auto_hop_mode_active = !manager.m_session_state.auto_hop_mode_active;
    if (manager.m_session_state.auto_hop_mode_active) {
        manager.m_session_state.last_switch_time = std::chrono::steady_clock::now();
        manager.m_session_state.auto_hop_start_time = std::chrono::steady_clock::now();
        if (!manager.m_stations.empty()) {
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

void ActionHandler::handle_toggleFavorite(StationManager& manager) {
    if (manager.m_session_state.active_station_idx >= 0 &&
        manager.m_session_state.active_station_idx < (int) manager.m_stations.size()) {
        manager.m_stations[manager.m_session_state.active_station_idx].toggleFavorite();
    }
    manager.m_needs_redraw = true;
}

void ActionHandler::handle_toggleDucking(StationManager& manager) {
    if (manager.m_session_state.active_station_idx < 0 ||
        manager.m_session_state.active_station_idx >= (int) manager.m_stations.size())
        return;
    RadioStream& station = manager.m_stations[manager.m_session_state.active_station_idx];
    if (!station.isInitialized() || station.getPlaybackState() == PlaybackState::Muted)
        return;
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

void ActionHandler::handle_toggleCopyMode(StationManager& manager) {
    manager.m_session_state.copy_mode_active = !manager.m_session_state.copy_mode_active;
    if (manager.m_session_state.copy_mode_active) {
        manager.m_session_state.copy_mode_start_time = std::chrono::steady_clock::now();
    }
    manager.m_needs_redraw = true;
}

void ActionHandler::handle_toggleHopperMode(StationManager& manager) {
    manager.m_session_state.hopper_mode = (manager.m_session_state.hopper_mode == HopperMode::PERFORMANCE)
                                              ? HopperMode::BALANCED
                                              : HopperMode::PERFORMANCE;
    manager.updateActiveWindow();
    manager.m_needs_redraw = true;
}

void ActionHandler::handle_switchPanel(StationManager& manager) {
    manager.m_session_state.active_panel =
        (manager.m_session_state.active_panel == ActivePanel::STATIONS) ? ActivePanel::HISTORY : ActivePanel::STATIONS;
    manager.m_needs_redraw = true;
}
