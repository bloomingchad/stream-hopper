#include "Core/SystemHandler.h"
#include "StationManager.h"
#include "Core/UpdateManager.h"
#include <chrono>
#include <variant>
#include <algorithm>

namespace {
    constexpr int COPY_MODE_TIMEOUT_SECONDS = 10;
    constexpr int FOCUS_MODE_SECONDS = 90;
    constexpr int AUTO_HOP_TOTAL_TIME_SECONDS = 1125;
    constexpr int FORGOTTEN_MUTE_SECONDS = 600;
}

void SystemHandler::process_system(StationManager& manager, const StationManagerMessage& msg) {
    std::visit([this, &manager](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if      constexpr (std::is_same_v<T, Msg::UpdateAndPoll>) handle_updateAndPoll(manager);
        else if constexpr (std::is_same_v<T, Msg::Quit>)         handle_quit(manager);
    }, msg);
}

void SystemHandler::handle_updateAndPoll(StationManager& manager) {
    manager.m_update_manager->process_updates(manager);
    manager.pollMpvEvents();

    auto now = std::chrono::steady_clock::now();
    if (manager.m_session_state.copy_mode_active) {
        if (std::chrono::duration_cast<std::chrono::seconds>(now - manager.m_session_state.copy_mode_start_time).count() >= COPY_MODE_TIMEOUT_SECONDS) {
            // Post a message to the queue instead of calling the handler directly
            manager.post(Msg::ToggleCopyMode{});
        }
    }
    if (manager.m_session_state.auto_hop_mode_active) {
        auto station_count = manager.m_stations.size();
        if (station_count > 0) {
            int duration = AUTO_HOP_TOTAL_TIME_SECONDS / static_cast<int>(station_count);
            if (std::chrono::duration_cast<std::chrono::seconds>(now - manager.m_session_state.auto_hop_start_time).count() >= duration) {
                // Post a message to the queue
                manager.post(Msg::NavigateDown{});
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
                    // Post a message to the queue
                    manager.post(Msg::Quit{});
                }
            }
        }
    }

    bool is_any_station_cycling = std::any_of(manager.m_stations.begin(), manager.m_stations.end(),
        [](const RadioStream& s) { return s.getCyclingState() == CyclingState::CYCLING; });

    if (manager.m_session_state.auto_hop_mode_active || is_any_station_cycling) {
        manager.m_needs_redraw = true;
    }
}

void SystemHandler::handle_quit(StationManager& manager) {
    manager.m_quit_flag = true;
}
