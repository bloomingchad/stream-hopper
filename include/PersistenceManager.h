#ifndef PERSISTENCEMANAGER_H
#define PERSISTENCEMANAGER_H

#include "nlohmann/json.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <optional>

// Forward declaration
class RadioStream;

class PersistenceManager {
public:
    PersistenceManager() = default;

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
