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
    nlohmann::json loadHistory();
    void saveHistory(const nlohmann::json& history_data);

    // Favorites Persistence
    std::unordered_set<std::string> loadFavoriteNames();
    void saveFavorites(const std::vector<RadioStream>& stations);

    // Session Persistence
    std::optional<std::string> loadLastStationName();
    void saveSession(const std::string& last_station_name);
};

#endif // PERSISTENCEMANAGER_H
