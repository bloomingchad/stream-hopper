#include "Core/MpvEventHandler.h"

#include <algorithm>
#include <cmath>
#include <cstring> // Required for strcmp
#include <ctime>
#include <iomanip>
#include <sstream>

#include "RadioStream.h"
#include "StationManager.h"
#include "UI/UIUtils.h" // For contains_ci
#include "Utils.h"      // For check_mpv_error
#include "nlohmann/json.hpp"

namespace {
    static constexpr char* PROP_NAME_MEDIA_TITLE = (char*) "media-title";
    static constexpr char* PROP_NAME_AUDIO_BITRATE = (char*) "audio-bitrate";
    static constexpr char* PROP_NAME_EOF_REACHED = (char*) "eof-reached";
    static constexpr char* PROP_NAME_CORE_IDLE = (char*) "core-idle";
    // PROP_NAME_DEMUXER_STATE was unused, so removed for now.

    constexpr int BITRATE_REDRAW_THRESHOLD = 2;
    constexpr int PENDING_INSTANCE_ID_OFFSET = 10000;
}

MpvEventHandler::MpvEventHandler(StationManager& manager) : m_manager(manager) {}

void MpvEventHandler::handleEvent(mpv_event* event) {
    if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
        handlePropertyChange(event);
    } else if (event->event_id == MPV_EVENT_END_FILE) {
        // This logic is specific and small, so it can stay here.
        if (event->reply_userdata >= PENDING_INSTANCE_ID_OFFSET) {
            int station_id = event->reply_userdata - PENDING_INSTANCE_ID_OFFSET;
            if (station_id >= 0 && station_id < (int) m_manager.m_stations.size()) {
                auto& station = m_manager.m_stations[station_id];
                if (station.getCyclingState() == CyclingState::CYCLING) {
                    station.finalizeCycle(false); // Cycle failed on EOF
                    m_manager.getNeedsRedrawFlag() = true;
                }
            }
        }
        // Note: EOF for main instance is handled by onEofProperty
    }
}

void MpvEventHandler::handle_pending_instance_property_change(mpv_event* event, mpv_event_property* prop) {
    int station_id = event->reply_userdata - PENDING_INSTANCE_ID_OFFSET;
    if (station_id < 0 || station_id >= (int) m_manager.m_stations.size())
        return;

    RadioStream& station = m_manager.m_stations[station_id];

    if (station.getCyclingState() != CyclingState::CYCLING) {
        // If no longer cycling, unobserve and ignore. This can happen if the cycle timed out or was cancelled.
        mpv_unobserve_property(station.getPendingMpvInstance().get(), station_id + PENDING_INSTANCE_ID_OFFSET);
        return;
    }

    bool property_changed_for_pending = false;
    if (strcmp(prop->name, PROP_NAME_MEDIA_TITLE) == 0) {
        if (prop->format == MPV_FORMAT_STRING) {
            char* title_cstr = *reinterpret_cast<char**>(prop->data);
            station.setPendingTitle(title_cstr ? std::string(title_cstr) : "");
            property_changed_for_pending = true;
        }
    } else if (strcmp(prop->name, PROP_NAME_AUDIO_BITRATE) == 0) {
        if (prop->format == MPV_FORMAT_INT64) {
            int new_bitrate = static_cast<int>(*reinterpret_cast<int64_t*>(prop->data) / 1000);
            if (new_bitrate > 0) {
                station.setPendingBitrate(new_bitrate);
                property_changed_for_pending = true;
            }
        }
    }

    if (property_changed_for_pending) {
        m_manager.getNeedsRedrawFlag() = true;
        // If we have both title and bitrate (or just bitrate if title never comes), proceed.
        // The primary trigger for crossfade is getting a valid pending bitrate.
        if (station.getPendingBitrate() > 0) {
            m_manager.crossFadeToPending(station_id);
            // Once we start the crossfade, we can stop observing.
            // The finalizeCycle(true) will happen in UpdateManager after fade completes.
            mpv_unobserve_property(station.getPendingMpvInstance().get(), station_id + PENDING_INSTANCE_ID_OFFSET);
        }
    }
}

void MpvEventHandler::handle_main_instance_property_change(mpv_event* event, mpv_event_property* prop) {
    RadioStream* station_ptr = findStationById(event->reply_userdata);
    if (!station_ptr || !station_ptr->isInitialized())
        return;

    RadioStream& station = *station_ptr;

    if (strcmp(prop->name, PROP_NAME_MEDIA_TITLE) == 0)
        onTitleProperty(prop, station);
    else if (strcmp(prop->name, PROP_NAME_AUDIO_BITRATE) == 0)
        onBitrateProperty(prop, station);
    else if (strcmp(prop->name, PROP_NAME_EOF_REACHED) == 0)
        onEofProperty(prop, station);
    else if (strcmp(prop->name, PROP_NAME_CORE_IDLE) == 0)
        onCoreIdleProperty(prop, station);
}

void MpvEventHandler::handlePropertyChange(mpv_event* event) {
    mpv_event_property* prop = reinterpret_cast<mpv_event_property*>(event->data);

    if (event->reply_userdata >= PENDING_INSTANCE_ID_OFFSET) {
        handle_pending_instance_property_change(event, prop);
    } else {
        handle_main_instance_property_change(event, prop);
    }
}

void MpvEventHandler::onTitleChanged(RadioStream& station, const std::string& new_title) {
    if (station.getCyclingState() == CyclingState::CYCLING)
        return; // Ignore title changes during an active cycle

    if (new_title.empty() || new_title == station.getCurrentTitle() || new_title == "N/A" ||
        new_title == "Initializing...")
        return;

    // Avoid logging station name or URL as song title
    if (contains_ci(station.getActiveUrl(), new_title) || contains_ci(station.getName(), new_title)) {
        if (new_title != station.getCurrentTitle()) { // Still update display if it's just the station name
            station.setCurrentTitle(new_title);
            m_manager.getNeedsRedrawFlag() = true;
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
    std::tm now_tm = *std::localtime(&now_c); // localtime is not thread-safe, but MPV events are on one thread
    std::stringstream time_ss;
    time_ss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");

    nlohmann::json history_entry_for_file = {time_ss.str(), title_to_log};
    m_manager.addHistoryEntry(station.getName(), history_entry_for_file);

    station.setCurrentTitle(new_title);
    m_manager.getNeedsRedrawFlag() = true;
}

void MpvEventHandler::onStreamEof(RadioStream& station) {
    // This is for the main instance. If a stream ends, try to reconnect.
    station.setCurrentTitle("Stream Error - Reconnecting...");
    station.setHasLoggedFirstSong(false); // Reset for the new connection attempt
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
        if (new_bitrate > 0)
            station.setBitrate(new_bitrate); // Only update if valid

        // Redraw if it's the active station and bitrate changed significantly
        if (station.getID() == m_manager.m_session_state.active_station_idx &&
            std::abs(new_bitrate - old_bitrate) > BITRATE_REDRAW_THRESHOLD) {
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
            if (station.getID() == m_manager.m_session_state.active_station_idx) {
                m_manager.getNeedsRedrawFlag() = true;
            }
        }
    }
}

RadioStream* MpvEventHandler::findStationById(int station_id) {
    auto& stations_vec = m_manager.m_stations; // Use a local ref for clarity
    auto it = std::find_if(stations_vec.begin(), stations_vec.end(),
                           [station_id](const RadioStream& s) { return s.getID() == station_id; });
    return (it != stations_vec.end()) ? &(*it) : nullptr;
}
