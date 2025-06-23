#include "PersistenceManager.h"

#include <fstream>
#include <iomanip>
#include <stdexcept> // For std::runtime_error

#include "RadioStream.h" // For RadioStream definition in saveFavorites

using nlohmann::json;

// Centralized filenames
const std::string STATIONS_FILENAME = "stations.jsonc";
const std::string FAVORITES_FILENAME = "radio_favorites.json";
const std::string SESSION_FILENAME = "radio_session.json";
const std::string HISTORY_FILENAME = "radio_history.json";

StationData PersistenceManager::loadStations() const {
    std::ifstream i(STATIONS_FILENAME);
    if (!i.is_open()) {
        throw std::runtime_error(
            "Could not open stations.jsonc. Please ensure the file exists in the same directory as the executable.");
    }

    StationData station_data;
    try {
        // The 'true' argument in the 4th position enables comment skipping.
        json stations_json = json::parse(i, nullptr, true, true);

        if (!stations_json.is_array()) {
            throw std::runtime_error("stations.jsonc must contain a top-level JSON array.");
        }

        for (const auto& station_entry : stations_json) {
            if (!station_entry.is_object() || !station_entry.contains("name") || !station_entry.contains("urls")) {
                continue; // Skip malformed entries silently
            }

            const auto& name_json = station_entry["name"];
            const auto& urls_json = station_entry["urls"];

            if (!name_json.is_string() || !urls_json.is_array() || urls_json.empty()) {
                continue; // Skip entries with invalid types or no URLs
            }

            std::string name = name_json.get<std::string>();
            std::vector<std::string> urls;
            for (const auto& url_entry : urls_json) {
                if (url_entry.is_string()) {
                    urls.push_back(url_entry.get<std::string>());
                }
            }

            if (!name.empty() && !urls.empty()) {
                station_data.emplace_back(name, urls);
            }
        }
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Failed to parse stations.jsonc: " + std::string(e.what()));
    }

    if (station_data.empty()) {
        throw std::runtime_error("stations.jsonc is empty or contains no valid station entries.");
    }

    return station_data;
}

json PersistenceManager::loadHistory() const {
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

void PersistenceManager::saveHistory(const json& history_data) const {
    std::ofstream o(HISTORY_FILENAME);
    if (o.is_open()) {
        o << std::setw(4) << history_data << std::endl;
    }
}

std::unordered_set<std::string> PersistenceManager::loadFavoriteNames() const {
    std::unordered_set<std::string> favorite_set;
    std::ifstream i(FAVORITES_FILENAME);
    if (!i.is_open())
        return favorite_set;

    json fav_names;
    try {
        i >> fav_names;
        if (!fav_names.is_array())
            return favorite_set;
    } catch (const json::parse_error&) {
        return favorite_set;
    }

    for (const auto& name_json : fav_names) {
        if (name_json.is_string()) {
            favorite_set.insert(name_json.get<std::string>());
        }
    }
    return favorite_set;
}

void PersistenceManager::saveFavorites(const std::vector<RadioStream>& stations) const {
    json fav_names = json::array();
    for (const auto& station : stations) {
        if (station.isFavorite()) {
            fav_names.push_back(station.getName());
        }
    }

    std::ofstream o(FAVORITES_FILENAME);
    if (o.is_open()) {
        o << std::setw(4) << fav_names << std::endl;
    }
}

std::optional<std::string> PersistenceManager::loadLastStationName() const {
    std::ifstream i(SESSION_FILENAME);
    if (!i.is_open())
        return std::nullopt;

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

void PersistenceManager::saveSession(const std::string& last_station_name) const {
    if (last_station_name.empty())
        return;
    json session_data;
    session_data["last_station_name"] = last_station_name;
    std::ofstream o(SESSION_FILENAME);
    if (o.is_open()) {
        o << std::setw(4) << session_data << std::endl;
    }
}
