#ifndef PERSISTENCEMANAGER_H
#define PERSISTENCEMANAGER_H

#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility> // For std::pair
#include <vector>

#include "CuratorStation.h"
#include "nlohmann/json.hpp"

// Forward declaration
class RadioStream;

// A type alias for clarity
using StationData = std::vector<std::pair<std::string, std::vector<std::string>>>;

class PersistenceManager {
  public:
    PersistenceManager() = default;

    StationData loadStations(const std::string& filename) const;
    void saveSimpleStationList(const std::string& filename, const std::vector<CuratorStation>& stations) const;

    // History Persistence
    nlohmann::json loadHistory() const;
    void saveHistory(const nlohmann::json& history_data) const;

    // Favorites Persistence
    std::unordered_set<std::string> loadFavoriteNames() const;
    void saveFavorites(const std::vector<RadioStream>& stations) const;

    // Session Persistence
    std::optional<std::string> loadLastStationName() const;
    void saveSession(const std::string& last_station_name) const;

    // Volume Offset Persistence
    std::map<std::string, double> loadVolumeOffsets() const;
    void saveVolumeOffsets(const std::map<std::string, double>& offsets) const;

  private:
    // Helper to parse a single station entry from the JSON array
    std::optional<std::pair<std::string, std::vector<std::string>>>
    parse_single_station_entry(const nlohmann::json& station_entry) const;
};

#endif // PERSISTENCEMANAGER_H
