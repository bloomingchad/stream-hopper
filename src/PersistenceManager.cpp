#include "PersistenceManager.h"
#include "RadioStream.h" // For RadioStream definition in saveFavorites
#include <fstream>
#include <iomanip>

using nlohmann::json;

// Centralized filenames
const std::string FAVORITES_FILENAME = "radio_favorites.json";
const std::string SESSION_FILENAME = "radio_session.json";
const std::string HISTORY_FILENAME = "radio_history.json";

json PersistenceManager::loadHistory() {
    std::ifstream i(HISTORY_FILENAME);
    if (i.is_open()) {
        try {
            json history_data;
            i >> history_data;
            if (history_data.is_object()) {
                return history_data;
            }
        } catch (...) {
            // Fall through to return an empty object on error
        }
    }
    return json::object();
}

void PersistenceManager::saveHistory(const json& history_data) {
    std::ofstream o(HISTORY_FILENAME);
    if (o.is_open()) {
        o << std::setw(4) << history_data << std::endl;
    }
}

std::unordered_set<std::string> PersistenceManager::loadFavoriteNames() {
    std::unordered_set<std::string> favorite_set;
    std::ifstream i(FAVORITES_FILENAME);
    if (!i.is_open()) return favorite_set;

    json fav_names;
    try {
        i >> fav_names;
        if (!fav_names.is_array()) return favorite_set;
    } catch (const json::parse_error &) { return favorite_set; }

    for (const auto &name_json : fav_names) {
        if (name_json.is_string()) {
            favorite_set.insert(name_json.get<std::string>());
        }
    }
    return favorite_set;
}

void PersistenceManager::saveFavorites(const std::vector<RadioStream>& stations) {
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

std::optional<std::string> PersistenceManager::loadLastStationName() {
    std::ifstream i(SESSION_FILENAME);
    if (!i.is_open()) return std::nullopt;

    try {
        json session_data;
        i >> session_data;
        if (session_data.is_object() && session_data.contains("last_station_name")) {
            return session_data["last_station_name"].get<std::string>();
        }
    } catch (const json::parse_error&) {
        // Silently ignore
    }
    return std::nullopt;
}

void PersistenceManager::saveSession(const std::string& last_station_name) {
    if (last_station_name.empty()) return;
    json session_data;
    session_data["last_station_name"] = last_station_name;
    std::ofstream o(SESSION_FILENAME);
    if (o.is_open()) {
        o << std::setw(4) << session_data << std::endl;
    }
}
