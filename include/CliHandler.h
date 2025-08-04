#ifndef CLIHANDLER_H
#define CLIHANDLER_H

#include <string>
#include <vector>

#include "CuratorStation.h"

class CliHandler {
  public:
    CliHandler() = default;

    // --- Existing CLI-facing methods ---
    void handle_list_tags();
    void handle_curate_genre(const std::string& genre);

    // --- New programmatic methods for the Wizard ---
    std::vector<std::string> get_curated_tags();
    std::vector<CuratorStation> get_curation_candidates(const std::string& genre);

    // --- New programmatic method for Random Mode ---
    std::vector<CuratorStation> get_random_stations(int limit);
};

#endif // CLIHANDLER_H
