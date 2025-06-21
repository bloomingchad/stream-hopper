#include "Core/MpvEventHandler.h"
#include "StationManager.h"
#include "RadioStream.h"
#include "Utils.h"
#include "UI/UIUtils.h"
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include "nlohmann/json.hpp"

namespace {
    static constexpr char* PROP_MEDIA_TITLE = (char*)"media-title";
    static constexpr char* PROP_AUDIO_BITRATE = (char*)"audio-bitrate";
    static constexpr char* PROP_EOF_REACHED = (char*)"eof-reached";
    static constexpr char* PROP_CORE_IDLE = (char*)"core-idle";
    static constexpr char* PROP_DEMUXER_STATE = (char*)"demuxer-cache-state";
    constexpr int BITRATE_REDRAW_THRESHOLD = 2;
    constexpr int PENDING_INSTANCE_ID_OFFSET = 10000;
}

MpvEventHandler::MpvEventHandler(StationManager& manager) : m_manager(manager) {}
void MpvEventHandler::handleEvent(mpv_event* event) {
    if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
        handlePropertyChange(event);
    } else if (event->event_id == MPV_EVENT_END_FILE) {
        if (event->reply_userdata >= PENDING_INSTANCE_ID_OFFSET) {
            int station_id = event->reply_userdata - PENDING_INSTANCE_ID_OFFSET;
            if (station_id < (int)m_manager.m_stations.size()) { // The 'station_id >= 0' check is redundant
                auto& station = m_manager.m_stations[station_id];
                if (station.getCyclingState() == CyclingState::CYCLING) {
                    station.finalizeCycle(false);
                    m_manager.getNeedsRedrawFlag() = true;
                }
            }
        }
    }
}

void MpvEventHandler::handlePropertyChange(mpv_event* event) {
    mpv_event_property* prop = reinterpret_cast<mpv_event_property*>(event->data);
    
    if (event->reply_userdata >= PENDING_INSTANCE_ID_OFFSET) {
        int station_id = event->reply_userdata - PENDING_INSTANCE_ID_OFFSET;
        if (station_id >= (int)m_manager.m_stations.size()) return; // The 'station_id < 0' check is redundant
        auto& station = m_manager.m_stations[station_id];

        if (station.getCyclingState() != CyclingState::CYCLING) {
            mpv_unobserve_property(station.getPendingMpvInstance().get(), station_id + PENDING_INSTANCE_ID_OFFSET);
            return;
        }

        bool property_changed = false;
        if (strcmp(prop->name, PROP_MEDIA_TITLE) == 0) {
            if (prop->format == MPV_FORMAT_STRING) {
                char* title_cstr = *reinterpret_cast<char**>(prop->data);
                station.setPendingTitle(title_cstr ? std::string(title_cstr) : "");
                property_changed = true;
            }
        } else if (strcmp(prop->name, PROP_AUDIO_BITRATE) == 0) {
            if (prop->format == MPV_FORMAT_INT64) {
                int new_bitrate = static_cast<int>(*reinterpret_cast<int64_t*>(prop->data) / 1000);
                if (new_bitrate > 0) {
                    station.setPendingBitrate(new_bitrate);
                    property_changed = true;
                }
            }
        }
        
        if (property_changed) {
            m_manager.getNeedsRedrawFlag() = true;
            
            if (station.getPendingBitrate() > 0) {
                 m_manager.crossFadeToPending(station_id);
                 mpv_unobserve_property(station.getPendingMpvInstance().get(), station_id + PENDING_INSTANCE_ID_OFFSET);
            }
        }
        return;
    }

    // Otherwise, handle as a normal event for a main instance
    RadioStream* station = findStationById(event->reply_userdata);
    if (!station || !station->isInitialized()) return;
    
    if (strcmp(prop->name, PROP_MEDIA_TITLE) == 0) onTitleProperty(prop, *station);
    else if (strcmp(prop->name, PROP_AUDIO_BITRATE) == 0) onBitrateProperty(prop, *station);
    else if (strcmp(prop->name, PROP_EOF_REACHED) == 0) onEofProperty(prop, *station);
    else if (strcmp(prop->name, PROP_CORE_IDLE) == 0) onCoreIdleProperty(prop, *station);
}

void MpvEventHandler::onTitleChanged(RadioStream& station, const std::string& new_title) {
    if (station.getCyclingState() == CyclingState::CYCLING) return;
    
    if (new_title.empty() || new_title == station.getCurrentTitle() || new_title == "N/A" || new_title == "Initializing...") return;
    if (contains_ci(station.getActiveUrl(), new_title) || contains_ci(station.getName(), new_title)) {
        if (new_title != station.getCurrentTitle()) station.setCurrentTitle(new_title);
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
    m_manager.addHistoryEntry(station.getName(), history_entry_for_file);
    station.setCurrentTitle(new_title);
    m_manager.getNeedsRedrawFlag() = true;
}

void MpvEventHandler::onStreamEof(RadioStream& station) {
    station.setCurrentTitle("Stream Error - Reconnecting...");
    station.setHasLoggedFirstSong(false);
    const char* cmd[] = {"loadfile", station.getActiveUrl().c_str(), "replace", nullptr};
    check_mpv_error(mpv_command_async(station.getMpvHandle(), 0, cmd), "reconnect on eof");
    m_manager.getNeedsRedrawFlag() = true;
}

void MpvEventHandler::onTitleProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_STRING) {
        char* title_cstr = *reinterpret_cast<char**>(prop->data);
        std::string title = title_cstr ? std::string(title_cstr) : "N/A";
        onTitleChanged(station, title);
    }
}

void MpvEventHandler::onBitrateProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_INT64) {
        int old_bitrate = station.getBitrate();
        int new_bitrate = static_cast<int>(*reinterpret_cast<int64_t*>(prop->data) / 1000);
        station.setBitrate(new_bitrate);
        if (station.getID() == m_manager.m_session_state.active_station_idx && std::abs(new_bitrate - old_bitrate) > BITRATE_REDRAW_THRESHOLD) {
            m_manager.getNeedsRedrawFlag() = true;
        }
    }
}

void MpvEventHandler::onEofProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_FLAG && *reinterpret_cast<int*>(prop->data)) {
        onStreamEof(station);
    }
}

void MpvEventHandler::onCoreIdleProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_FLAG) {
        bool is_idle = *reinterpret_cast<int*>(prop->data);
        if (station.isBuffering() != is_idle) {
            station.setBuffering(is_idle);
            m_manager.getNeedsRedrawFlag() = true;
        }
    }
}

RadioStream* MpvEventHandler::findStationById(int station_id) {
    auto& stations = m_manager.m_stations;
    auto it = std::find_if(stations.begin(), stations.end(), [station_id](const RadioStream& s) { return s.getID() == station_id; });
    return (it != stations.end()) ? &(*it) : nullptr;
}
