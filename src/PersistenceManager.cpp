#include "PersistenceManager.h"

#include <fstream>
#include <iomanip>
#include <stdexcept> // For std::runtime_error

#include "RadioStream.h"
#include "nlohmann/json.hpp"

using nlohmann::json;

// Centralized filenames for non-station data
const std::string FAVORITES_FILENAME = "radio_favorites.json";
const std::string SESSION_FILENAME = "radio_session.json";
const std::string HISTORY_FILENAME = "radio_history.json";
const std::string VOLUME_OFFSETS_FILENAME = "volume_offsets.jsonc";

std::optional<std::pair<std::string, std::vector<std::string>>>
PersistenceManager::parse_single_station_entry(const json& station_entry) const {
    if (!station_entry.is_object() || !station_entry.contains("name") || !station_entry.contains("urls")) {
        return std::nullopt; // Not a valid station object
    }

    const auto& name_json = station_entry["name"];
    const auto& urls_json = station_entry["urls"];

    if (!name_json.is_string() || !urls_json.is_array() || urls_json.empty()) {
        return std::nullopt; // Invalid name or urls structure
    }

    std::string name = name_json.get<std::string>();
    if (name.empty()) {
        return std::nullopt; // Name cannot be empty
    }

    std::vector<std::string> urls;
    for (const auto& url_entry : urls_json) {
        if (url_entry.is_string()) {
            std::string url_str = url_entry.get<std::string>();
            if (!url_str.empty()) { // Ensure URL string is not empty
                urls.push_back(url_str);
            }
        }
    }

    if (urls.empty()) {
        return std::nullopt; // Must have at least one valid URL
    }

    return std::make_pair(name, urls);
}

StationData PersistenceManager::loadStations(const std::string& filename) const {
    std::ifstream i(filename);
    if (!i.is_open()) {
        throw std::runtime_error("Could not open station file: " + filename +
                                 ". Please ensure the file exists in the same directory as the executable.");
    }

    StationData station_data_list;
    try {
        json root_json = json::parse(i, nullptr, true, true); // Allow comments
        if (!root_json.is_array()) {
            throw std::runtime_error(filename + " must contain a top-level JSON array.");
        }
        for (const auto& station_entry_json : root_json) {
            if (auto parsed_station = parse_single_station_entry(station_entry_json)) {
                station_data_list.push_back(*parsed_station);
            }
            // If parse_single_station_entry returns nullopt, we silently skip the invalid entry.
            // This makes loading more robust to malformed entries in user-provided files.
        }
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Failed to parse " + filename + ": " + std::string(e.what()));
    }

    if (station_data_list.empty()) {
        throw std::runtime_error(filename + " is empty or contains no valid station entries.");
    }
    return station_data_list;
}

void PersistenceManager::saveSimpleStationList(const std::string& filename,
                                               const std::vector<CuratorStation>& stations) const {
    json stations_json_array = json::array();
    for (const auto& station_data : stations) {
        json station_obj;
        station_obj["name"] = station_data.name;
        station_obj["urls"] = station_data.urls;
        stations_json_array.push_back(station_obj);
    }

    std::ofstream o(filename);
    if (o.is_open()) {
        o << std::setw(4) << stations_json_array << std::endl;
    }
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
            // Silently ignore parse errors for history, return empty object
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
        return favorite_set; // Silently ignore parse errors
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

std::map<std::string, double> PersistenceManager::loadVolumeOffsets() const {
    std::map<std::string, double> offsets;
    std::ifstream i(VOLUME_OFFSETS_FILENAME);
    if (!i.is_open()) {
        return offsets;
    }
    try {
        json data = json::parse(i, nullptr, true, true);
        if (data.is_object()) {
            for (auto& [key, value] : data.items()) {
                if (value.is_number()) {
                    offsets[key] = value.get<double>();
                }
            }
        }
    } catch (const json::parse_error&) {
        // Silently ignore parse errors for this non-critical file
    }
    return offsets;
}

void PersistenceManager::saveVolumeOffsets(const std::map<std::string, double>& offsets) const {
    json data = offsets;
    std::ofstream o(VOLUME_OFFSETS_FILENAME);
    if (o.is_open()) {
        o << std::setw(4) << data << std::endl;
    }
}
