#ifndef PERSISTENCEMANAGER_H
#define PERSISTENCEMANAGER_H

#include <optional>
#include <string>
#include <unordered_set>
#include <utility> // For std::pair
#include <vector>

#include "CuratorStation.h" // <<< THIS IS THE KEY
#include "nlohmann/json.hpp"

// Forward declaration
class RadioStream;

// A type alias for clarity
using StationData = std::vector<std::pair<std::string, std::vector<std::string>>>;

class PersistenceManager {
  public:
    PersistenceManager() = default;

    StationData loadStations(const std::string& filename) const;
    // <<< FIX: The declaration must be here.
    std::vector<CuratorStation> loadCurationCandidates(const std::string& filename) const;

    // History Persistence
    nlohmann::json loadHistory() const;
    void saveHistory(const nlohmann::json& history_data) const;

    // Favorites Persistence
    std::unordered_set<std::string> loadFavoriteNames() const;
    void saveFavorites(const std::vector<RadioStream>& stations) const;

    // Session Persistence
    std::optional<std::string> loadLastStationName() const;
    void saveSession(const std::string& last_station_name) const;
};

#endif // PERSISTENCEMANAGER_H
