#include "StationManager.h"

#include <mpv/client.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "Core/ActionHandler.h"
#include "Core/MpvEventHandler.h"
#include "Core/SystemHandler.h"
#include "Core/UpdateManager.h"
#include "PersistenceManager.h"
#include "SessionState.h"
#include "Utils.h"

namespace {
    constexpr auto ACTOR_LOOP_TIMEOUT = std::chrono::milliseconds(20);
    constexpr int CROSSFADE_TIME_MS = 1200;
    const std::string SEARCH_PROVIDERS_FILENAME = "search_providers.jsonc";
}

void StationManager::loadSearchProviders() {
    std::ifstream i(SEARCH_PROVIDERS_FILENAME);
    if (!i.is_open()) {
        // Log an error but don't crash, the feature will just be disabled
        std::cerr << "Warning: Could not open " << SEARCH_PROVIDERS_FILENAME << ". Search feature will be disabled."
                  << std::endl;
        return;
    }

    try {
        nlohmann::json providers_json = nlohmann::json::parse(i, nullptr, true, true);
        for (const auto& entry : providers_json) {
            std::string key_str = entry.at("key").get<std::string>();
            if (key_str.length() == 1) {

                UrlEncodingStyle style = UrlEncodingStyle::UNKNOWN;
                std::string style_str = entry.at("encoding_style").get<std::string>();
                if (style_str == "query_plus")
                    style = UrlEncodingStyle::QUERY_PLUS;
                else if (style_str == "path_percent")
                    style = UrlEncodingStyle::PATH_PERCENT;
                else if (style_str == "bandcamp_special")
                    style = UrlEncodingStyle::BANDCAMP_SPECIAL;

                SearchProvider p = {.name = entry.at("name").get<std::string>(),
                                    .key = key_str[0],
                                    .base_url = entry.at("base_url").get<std::string>(),
                                    .encoding_style = style};
                m_search_providers[p.key] = p;
            }
        }
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Warning: Failed to parse " << SEARCH_PROVIDERS_FILENAME << ": " << e.what()
                  << ". Search feature may be incomplete." << std::endl;
    }
}

StationManager::StationManager(const StationData& station_data)
    : m_unsaved_history_count(0), m_session_state(), m_quit_flag(false), m_needs_redraw(true) {
    if (station_data.empty()) {
        throw std::runtime_error("No radio stations provided.");
    }

    loadSearchProviders(); // Load the new config

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
            m_session_state.active_station_idx = std::distance(m_stations.begin(), it);
        }
    }

    m_event_handler = std::make_unique<MpvEventHandler>(*this);
    m_action_handler = std::make_unique<ActionHandler>();
    m_system_handler = std::make_unique<SystemHandler>();
    m_update_manager = std::make_unique<UpdateManager>();
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
    if (!m_stations.empty() && m_session_state.active_station_idx >= 0 &&
        m_session_state.active_station_idx < (int) m_stations.size()) {
        persistence.saveSession(m_stations[m_session_state.active_station_idx].getName());
    }
    if (m_session_state.was_quit_by_mute_timeout) {
        constexpr int FORGOTTEN_MUTE_SECONDS = 600;
        std::cout << "Hey, you forgot about me for " << (FORGOTTEN_MUTE_SECONDS / 60) << " minutes! 😤" << std::endl;
    } else {
        auto end_time = std::chrono::steady_clock::now();
        auto duration_seconds =
            std::chrono::duration_cast<std::chrono::seconds>(end_time - m_session_state.session_start_time).count();
        long duration_minutes = duration_seconds / 60;
        std::cout << "---\n"
                  << "Thank you for using Stream Hopper!\n"
                  << "🎛️ Session Switches: " << m_session_state.session_switches << "\n"
                  << "✨ New Songs Found: " << m_session_state.new_songs_found << "\n"
                  << "📋 Songs Searched: " << m_session_state.songs_copied << "\n"
                  << "🕐 Total Time: " << duration_minutes << " minutes\n"
                  << "---" << std::endl;
    }
}

StateSnapshot StationManager::createSnapshot() const {
    std::lock_guard<std::mutex> lock(m_stations_mutex);
    StateSnapshot snapshot;
    snapshot.active_station_idx = m_session_state.active_station_idx;
    snapshot.active_panel = m_session_state.active_panel;
    snapshot.is_copy_mode_active = m_session_state.copy_mode_active;
    snapshot.is_auto_hop_mode_active = m_session_state.auto_hop_mode_active;
    snapshot.history_scroll_offset = m_session_state.history_scroll_offset;
    snapshot.hopper_mode = m_session_state.hopper_mode;
    snapshot.temporary_status_message = m_session_state.temporary_status_message;
    snapshot.stations.reserve(m_stations.size());
    for (const auto& station : m_stations) {
        snapshot.stations.push_back({.name = station.getName(),
                                     .current_title = station.getCurrentTitle(),
                                     .bitrate = station.getBitrate(),
                                     .current_volume = station.getCurrentVolume(),
                                     .is_initialized = station.isInitialized(),
                                     .is_favorite = station.isFavorite(),
                                     .is_buffering = station.isBuffering(),
                                     .playback_state = station.getPlaybackState(),
                                     .cycling_state = station.getCyclingState(),
                                     .pending_title = station.getPendingTitle(),
                                     .pending_bitrate = station.getPendingBitrate(),
                                     .url_count = station.getAllUrls().size()});
    }
    snapshot.current_volume_for_header = 0.0;
    if (!snapshot.stations.empty()) {
        const auto& active_station_data = snapshot.stations[snapshot.active_station_idx];
        if (active_station_data.is_initialized) {
            snapshot.current_volume_for_header =
                active_station_data.playback_state == PlaybackState::Muted ? 0.0 : active_station_data.current_volume;
        }
        const auto& active_station_name = active_station_data.name;
        if (m_song_history->contains(active_station_name)) {
            snapshot.active_station_history = (*m_song_history)[active_station_name];
        } else {
            snapshot.active_station_history = nlohmann::json::array();
        }
    }
    snapshot.auto_hop_total_duration = 0;
    if (!m_stations.empty()) {
        constexpr int AUTO_HOP_TOTAL_TIME_SECONDS = 1125;
        snapshot.auto_hop_total_duration = AUTO_HOP_TOTAL_TIME_SECONDS / static_cast<int>(m_stations.size());
    }
    snapshot.auto_hop_remaining_seconds = 0;
    if (m_session_state.auto_hop_mode_active) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_s =
            std::chrono::duration_cast<std::chrono::seconds>(now - m_session_state.auto_hop_start_time).count();
        snapshot.auto_hop_remaining_seconds =
            std::max(0, snapshot.auto_hop_total_duration - static_cast<int>(elapsed_s));
    }
    return snapshot;
}

void StationManager::post(StationManagerMessage message) {
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_message_queue.push_back(std::move(message));
    }
    m_queue_cond.notify_one();
}

std::atomic<bool>& StationManager::getQuitFlag() { return m_quit_flag; }
std::atomic<bool>& StationManager::getNeedsRedrawFlag() { return m_needs_redraw; }

void StationManager::actorLoop() {
    updateActiveWindow();
    while (!m_quit_flag) {
        std::deque<StationManagerMessage> current_queue;
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            if (m_queue_cond.wait_for(lock, ACTOR_LOOP_TIMEOUT,
                                      [this] { return !m_message_queue.empty() || m_quit_flag; })) {
                current_queue.swap(m_message_queue);
            }
        }
        if (m_quit_flag)
            break;
        std::lock_guard<std::mutex> lock(m_stations_mutex);
        if (current_queue.empty()) {
            current_queue.push_back(Msg::UpdateAndPoll{});
        }
        for (auto& msg : current_queue) {
            if (std::holds_alternative<Msg::UpdateAndPoll>(msg) || std::holds_alternative<Msg::Quit>(msg)) {
                m_system_handler->process_system(*this, msg);
            } else {
                m_action_handler->process_action(*this, msg);
            }
            if (m_quit_flag)
                break;
        }
    }
    for (int station_idx : m_active_station_indices) {
        if (station_idx >= 0 && station_idx < (int) m_stations.size()) {
            m_stations[station_idx].shutdown();
        }
    }
    m_active_station_indices.clear();
    m_active_fades.clear();
}

void StationManager::crossFadeToPending(int station_id) {
    if (station_id < 0 || station_id >= (int) m_stations.size())
        return;
    fadeAudio(station_id, 0.0, CROSSFADE_TIME_MS, false);
    fadeAudio(station_id, 100.0, CROSSFADE_TIME_MS, true);
}

void StationManager::pollMpvEvents() {
    bool events_pending = true;
    while (events_pending) {
        events_pending = false;

        const auto& indices_to_poll = m_active_station_indices;
        for (int station_idx : indices_to_poll) {
            if (station_idx >= (int) m_stations.size() || !m_stations[station_idx].isInitialized())
                continue;
            mpv_event* event = mpv_wait_event(m_stations[station_idx].getMpvHandle(), 0);
            if (event->event_id != MPV_EVENT_NONE) {
                m_event_handler->handleEvent(event);
                events_pending = true;
            }
        }

        if (m_session_state.active_station_idx >= 0 && m_session_state.active_station_idx < (int) m_stations.size()) {
            auto& station = m_stations[m_session_state.active_station_idx];
            if (station.getCyclingState() == CyclingState::CYCLING && station.getPendingMpvInstance().get()) {
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
    if (station_id < 0 || station_id >= (int) m_stations.size())
        return;

    m_active_fades.erase(std::remove_if(m_active_fades.begin(), m_active_fades.end(),
                                        [station_id, for_pending](const ActiveFade& f) {
                                            return f.station_id == station_id &&
                                                   f.is_for_pending_instance == for_pending;
                                        }),
                         m_active_fades.end());

    RadioStream& station = m_stations[station_id];
    mpv_handle* handle = for_pending ? station.getPendingMpvInstance().get() : station.getMpvHandle();
    if (!handle)
        return;

    m_active_fades.push_back({.station_id = station_id,
                              .generation = station.getGeneration(),
                              .start_vol = for_pending ? 0.0 : station.getCurrentVolume(),
                              .target_vol = to_vol,
                              .start_time = std::chrono::steady_clock::now(),
                              .duration_ms = duration_ms,
                              .is_for_pending_instance = for_pending});
}

void StationManager::updateActiveWindow() {
    const auto new_active_set =
        m_preloader.calculate_active_indices(m_session_state.active_station_idx, m_stations.size(),
                                             m_session_state.hopper_mode, m_session_state.nav_history);
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
    if (m_session_state.active_station_idx >= 0 && m_session_state.active_station_idx < (int) m_stations.size()) {
        RadioStream& new_station = m_stations[m_session_state.active_station_idx];
        constexpr int FADE_TIME_MS = 900;
        if (new_station.isInitialized() && new_station.getPlaybackState() != PlaybackState::Muted &&
            new_station.getCurrentVolume() < 99.0) {
            fadeAudio(m_session_state.active_station_idx, 100.0, FADE_TIME_MS, false);
        }
    }
}

void StationManager::initializeStation(int station_idx) {
    if (station_idx < 0 || station_idx >= (int) m_stations.size())
        return;
    double vol = (station_idx == m_session_state.active_station_idx) ? 100.0 : 0.0;
    m_stations[station_idx].initialize(vol);
    m_active_station_indices.insert(station_idx);
}

void StationManager::shutdownStation(int station_idx) {
    if (station_idx < 0 || station_idx >= (int) m_stations.size())
        return;
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
    m_session_state.new_songs_found++;
    if (++m_unsaved_history_count >= HISTORY_WRITE_THRESHOLD) {
        saveHistoryToDisk();
    }
}
