#include "AppState.h"
#include <algorithm>
#include "RadioStream.h"

using nlohmann::json;

AppState::AppState()
    : active_station_idx(0), quit_flag(false), needs_redraw(true),
      auto_hop_mode_active(false), copy_mode_active(false),
      active_panel(ActivePanel::STATIONS), history_scroll_offset(0),
      hopper_mode(HopperMode::BALANCED),
      last_switch_time(std::chrono::steady_clock::now()),
      session_start_time(std::chrono::steady_clock::now()),
      session_switches(0),
      new_songs_found(0),
      songs_copied(0),
      unsaved_history_count(0)
{
  m_song_history = std::make_unique<json>(json::object());
}

json& AppState::getHistory() { return *m_song_history; }

void AppState::setHistory(json&& history_data) {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    if (!history_data.is_object()) {
        *m_song_history = json::object();
    } else {
        *m_song_history = std::move(history_data);
    }
}

json AppState::getFullHistory() const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    return *m_song_history;
}

void AppState::addHistoryEntry(const std::string& station_name, const json& entry) {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    getHistory()[station_name].push_back(entry);
}

json AppState::getStationHistory(const std::string& station_name) const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    if (m_song_history->contains(station_name)) {
        return (*m_song_history)[station_name];
    }
    return json::array();
}

size_t AppState::getStationHistorySize(const std::string& station_name) const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    if (m_song_history->contains(station_name)) {
        return (*m_song_history)[station_name].size();
    }
    return 0;
}

void AppState::ensureStationHistoryExists(const std::string& station_name) {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    if (!m_song_history->contains(station_name)) {
        (*m_song_history)[station_name] = json::array();
    }
}
