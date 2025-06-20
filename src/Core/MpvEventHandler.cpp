#include "Core/MpvEventHandler.h"
#include "AppState.h"
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

    constexpr int BITRATE_REDRAW_THRESHOLD = 2;
    constexpr int HISTORY_WRITE_THRESHOLD = 5;
}

MpvEventHandler::MpvEventHandler(
    std::vector<RadioStream>& stations,
    AppState& app_state,
    std::function<void(StationManagerMessage)> poster
) : m_stations(stations), m_app_state(app_state), m_poster(std::move(poster)) {}

void MpvEventHandler::handleEvent(mpv_event* event) {
    if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
        handlePropertyChange(event);
    }
}

void MpvEventHandler::handlePropertyChange(mpv_event* event) {
    mpv_event_property* prop = reinterpret_cast<mpv_event_property*>(event->data);
    RadioStream* station = findStationById(event->reply_userdata);
    if (!station || !station->isInitialized()) return;
    
    if (strcmp(prop->name, PROP_MEDIA_TITLE) == 0) onTitleProperty(prop, *station);
    else if (strcmp(prop->name, PROP_AUDIO_BITRATE) == 0) onBitrateProperty(prop, *station);
    else if (strcmp(prop->name, PROP_EOF_REACHED) == 0) onEofProperty(prop, *station);
    else if (strcmp(prop->name, PROP_CORE_IDLE) == 0) onCoreIdleProperty(prop, *station);
}

void MpvEventHandler::onTitleChanged(RadioStream& station, const std::string& new_title) {
    if (new_title.empty() || new_title == station.getCurrentTitle() || new_title == "N/A" || new_title == "Initializing...") return;
    if (contains_ci(station.getURL(), new_title) || contains_ci(station.getName(), new_title)) {
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
    m_app_state.addHistoryEntry(station.getName(), history_entry_for_file);
    m_app_state.new_songs_found++;
    station.setCurrentTitle(new_title);
    m_app_state.needs_redraw = true;

    if (++m_app_state.unsaved_history_count >= HISTORY_WRITE_THRESHOLD) {
        if(m_poster) m_poster(Msg::SaveHistory{});
    }
}

void MpvEventHandler::onStreamEof(RadioStream& station) {
    station.setCurrentTitle("Stream Error - Reconnecting...");
    station.setHasLoggedFirstSong(false);
    m_app_state.needs_redraw = true;
    const char* cmd[] = {"loadfile", station.getURL().c_str(), "replace", nullptr};
    check_mpv_error(mpv_command_async(station.getMpvHandle(), 0, cmd), "reconnect on eof");
}

void MpvEventHandler::onTitleProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_STRING) {
        char* title_cstr = *reinterpret_cast<char**>(prop->data);
        // ** THE FIX **
        // Immediately copy the C-string to a std::string to avoid use-after-free,
        // as the data pointer is only valid for the lifetime of the event.
        std::string title = title_cstr ? std::string(title_cstr) : "N/A";
        onTitleChanged(station, title);
    }
}
void MpvEventHandler::onBitrateProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_INT64) {
        int old_bitrate = station.getBitrate();
        int new_bitrate = static_cast<int>(*reinterpret_cast<int64_t*>(prop->data) / 1000);
        station.setBitrate(new_bitrate);
        if (station.getID() == m_app_state.active_station_idx && std::abs(new_bitrate - old_bitrate) > BITRATE_REDRAW_THRESHOLD) {
            m_app_state.needs_redraw = true;
        }
    }
}
void MpvEventHandler::onEofProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_FLAG && *reinterpret_cast<int*>(prop->data)) onStreamEof(station);
}
void MpvEventHandler::onCoreIdleProperty(mpv_event_property* prop, RadioStream& station) {
    if (prop->format == MPV_FORMAT_FLAG) {
        bool is_idle = *reinterpret_cast<int*>(prop->data);
        if (station.isBuffering() != is_idle) {
            station.setBuffering(is_idle);
            m_app_state.needs_redraw = true;
        }
    }
}

RadioStream* MpvEventHandler::findStationById(int station_id) {
    auto it = std::find_if(m_stations.begin(), m_stations.end(), [station_id](const RadioStream& s) { return s.getID() == station_id; });
    return (it != m_stations.end()) ? &(*it) : nullptr;
}
