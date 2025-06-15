#include "AppState.h"
#include "RadioStream.h" // For persistence logic
#include <fstream>
#include <iomanip>
#include <unordered_set>
#include <algorithm>

using nlohmann::json;

const std::string FAVORITES_FILENAME = "radio_favorites.json";
const std::string SESSION_FILENAME = "radio_session.json";
const std::string HISTORY_FILENAME = "radio_history.json";

AppState::AppState()
    : active_station_idx(0), quit_flag(false), needs_redraw(true),
      small_mode_active(false), copy_mode_active(false),
      active_panel(ActivePanel::STATIONS), history_scroll_offset(0) {
  m_song_history = std::make_unique<json>(json::object());
}

json& AppState::getHistory() { return *m_song_history; }

void AppState::loadHistoryFromDisk() {
  std::ifstream i(HISTORY_FILENAME);
  if (i.is_open()) {
    try {
      i >> *m_song_history;
      if (!m_song_history->is_object()) {
        *m_song_history = json::object();
      }
    } catch (...) {
      *m_song_history = json::object();
    }
  }
}

void AppState::saveHistoryToDisk() {
  std::lock_guard<std::mutex> lock(m_history_mutex);
  std::ofstream o(HISTORY_FILENAME);
  if (o.is_open()) {
    o << std::setw(4) << *m_song_history << std::endl;
  }
}

void AppState::loadFavorites(std::vector<RadioStream>& stations) {
  std::ifstream i(FAVORITES_FILENAME);
  if (!i.is_open()) return;

  json fav_names;
  try {
    i >> fav_names;
    if (!fav_names.is_array()) return;
  } catch (const json::parse_error &) { return; }

  std::unordered_set<std::string> favorite_set;
  for (const auto &name_json : fav_names) {
    if (name_json.is_string()) {
      favorite_set.insert(name_json.get<std::string>());
    }
  }

  for (auto &station : stations) {
    if (favorite_set.count(station.getName())) {
      station.toggleFavorite();
    }
  }
}

void AppState::saveFavorites(const std::vector<RadioStream>& stations) {
  json fav_names = json::array();
  for (const auto &station : stations) {
    if (station.isFavorite()) {
      fav_names.push_back(station.getName());
    }
  }

  std::ofstream o(FAVORITES_FILENAME);
  if (o.is_open()) {
    o << std::setw(4) << fav_names << std::endl;
  }
}

void AppState::loadSession(const std::vector<RadioStream>& stations) {
    std::ifstream i(SESSION_FILENAME);
    if (!i.is_open()) return;

    try {
        json session_data;
        i >> session_data;
        if (session_data.is_object() && session_data.contains("last_station_name")) {
            std::string last_station_name = session_data["last_station_name"].get<std::string>();
            auto it = std::find_if(stations.begin(), stations.end(),
                                   [&last_station_name](const RadioStream& station) {
                                       return station.getName() == last_station_name;
                                   });
            if (it != stations.end()) {
                active_station_idx = std::distance(stations.begin(), it);
            }
        }
    } catch (const json::parse_error&) {
        // Silently ignore
    }
}

void AppState::saveSession(const std::vector<RadioStream>& stations) {
    if (stations.empty()) return;
    json session_data;
    session_data["last_station_name"] = stations[active_station_idx].getName();
    std::ofstream o(SESSION_FILENAME);
    if (o.is_open()) {
        o << std::setw(4) << session_data << std::endl;
    }
}
