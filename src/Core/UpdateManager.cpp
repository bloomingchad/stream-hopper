#include "Core/UpdateManager.h"

#include <algorithm> // For std::any_of, std::remove_if
#include <chrono>

#include "Core/VolumeNormalizer.h"
#include "PersistenceManager.h" // For StationData
#include "RadioStream.h"
#include "StationManager.h"

namespace {
    // Constants related to update logic
    constexpr int CYCLE_TIMEOUT_SECONDS = 8;
    constexpr int RANDOM_STATIONS_TARGET_COUNT = 15;
}

void UpdateManager::process_updates(StationManager& manager) {
    handle_random_station_fetch(manager);
    handle_temporary_message_timer(manager);
    handle_cycle_status_timers(manager);
    handle_cycle_timeouts(manager);
    handle_activeFades(manager);
    handle_volume_normalizer_timeout(manager);
    if (manager.m_is_fetching_random_stations) {
        manager.m_needs_redraw = true; // Keep UI animating while spinner is active
    }
}

void UpdateManager::handle_random_station_fetch(StationManager& manager) {
    if (!manager.m_is_fetching_random_stations || !manager.m_random_stations_future.valid()) {
        return;
    }

    // Check if the future is ready without blocking
    if (manager.m_random_stations_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto fetched_stations = manager.m_random_stations_future.get();
        manager.m_is_fetching_random_stations = false;

        if (fetched_stations.empty()) {
            manager.m_session_state.temporary_status_message = "[Error] Failed to fetch stations.";
            manager.m_session_state.temporary_message_end_time =
                std::chrono::steady_clock::now() + std::chrono::seconds(3);
        } else {
            StationData new_station_data;
            for (const auto& s : fetched_stations) {
                if (manager.m_seen_random_station_uuids.find(s.stationuuid) ==
                    manager.m_seen_random_station_uuids.end()) {
                    new_station_data.push_back({s.name, s.urls});
                    manager.m_seen_random_station_uuids.insert(s.stationuuid);
                    if (new_station_data.size() >= RANDOM_STATIONS_TARGET_COUNT) {
                        break;
                    }
                }
            }

            if (manager.m_fetch_is_for_append) {
                if (!new_station_data.empty()) {
                    manager.appendStations(new_station_data);
                }
            } else {
                manager.resetWithNewStations(new_station_data);
            }
        }
        manager.m_needs_redraw = true;
    }
}

void UpdateManager::handle_volume_normalizer_timeout(StationManager& manager) {
    if (manager.m_volume_normalizer->checkTimeout()) {
        manager.post(Msg::SaveVolumeOffsets{});
        manager.m_needs_redraw = true;
    }
}

void UpdateManager::handle_temporary_message_timer(StationManager& manager) {
    if (auto end_time = manager.m_session_state.temporary_message_end_time) {
        if (std::chrono::steady_clock::now() >= *end_time) {
            manager.m_session_state.temporary_status_message.clear();
            manager.m_session_state.temporary_message_end_time = std::nullopt;
            manager.m_needs_redraw = true;
        }
    }
}

void UpdateManager::handle_cycle_status_timers(StationManager& manager) {
    for (auto& station : manager.m_stations) {
        if (station.getCyclingState() == CyclingState::SUCCEEDED || station.getCyclingState() == CyclingState::FAILED) {
            if (std::chrono::steady_clock::now() >= station.getCycleStatusEndTime()) {
                station.clearCycleStatus();
                manager.m_needs_redraw = true;
            }
        }
    }
}

void UpdateManager::handle_cycle_timeouts(StationManager& manager) {
    auto now = std::chrono::steady_clock::now();
    for (auto& station : manager.m_stations) {
        if (station.getCyclingState() == CyclingState::CYCLING) {
            if (auto start_time = station.getCycleStartTime()) {
                if (std::chrono::duration_cast<std::chrono::seconds>(now - *start_time).count() >=
                    CYCLE_TIMEOUT_SECONDS) {
                    station.finalizeCycle(false);
                    manager.m_needs_redraw = true;
                }
            }
        }
    }
}

void UpdateManager::handle_activeFades(StationManager& manager) {
    if (manager.m_active_fades.empty())
        return;
    auto now = std::chrono::steady_clock::now();
    bool changed = false;

    manager.m_active_fades.erase(
        std::remove_if(manager.m_active_fades.begin(), manager.m_active_fades.end(),
                       [&](StationManager::ActiveFade& fade) -> bool {
                           if (fade.station_id < 0 || fade.station_id >= (int) manager.m_stations.size()) {
                               return true;
                           }
                           RadioStream& station = manager.m_stations[fade.station_id];

                           if (station.getGeneration() != fade.generation) {
                               return true;
                           }

                           mpv_handle* handle = fade.is_for_pending_instance ? station.getPendingMpvInstance().get()
                                                                             : station.getMpvHandle();
                           if (!handle) {
                               return true;
                           }

                           auto elapsed_ms =
                               std::chrono::duration_cast<std::chrono::milliseconds>(now - fade.start_time).count();
                           double progress = (fade.duration_ms > 0)
                                                 ? std::min(1.0, static_cast<double>(elapsed_ms) / fade.duration_ms)
                                                 : 1.0;
                           double new_vol = fade.start_vol + (fade.target_vol - fade.start_vol) * progress;

                           if (!fade.is_for_pending_instance) {
                               station.setCurrentVolume(new_vol);
                               manager.applyCombinedVolume(fade.station_id);
                           } else {
                               double final_pending_vol = std::clamp(new_vol, 0.0, 150.0);
                               mpv_set_property_async(handle, 0, "volume", MPV_FORMAT_DOUBLE, &final_pending_vol);
                           }
                           changed = true;

                           if (progress >= 1.0) {
                               if (fade.is_for_pending_instance) {
                                   station.promotePendingMetadata();
                                   station.promotePendingToActive();
                                   station.setCurrentVolume(fade.target_vol);
                                   manager.applyCombinedVolume(fade.station_id);
                                   station.finalizeCycle(true);
                               } else if (station.getCyclingState() == CyclingState::SUCCEEDED) {
                                   station.getPendingMpvInstance().shutdown();
                               }
                               return true;
                           }
                           return false;
                       }),
        manager.m_active_fades.end());

    if (changed)
        manager.m_needs_redraw = true;
}
