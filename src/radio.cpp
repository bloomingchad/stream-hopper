#include <fcntl.h>
#include <ncurses.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "PersistenceManager.h"
#include "RadioPlayer.h"
#include "StationManager.h"
#include "Utils.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

void suppress_stderr() {
    int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null == -1) {
        return;
    }
    dup2(dev_null, 2);
    close(dev_null);
}

// Normalizes and cleans a single tag name.
std::string normalize_tag(std::string tag) {
    std::transform(tag.begin(), tag.end(), tag.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Canonical mapping for common variations
    if (tag == "dnb" || tag == "drum and bass" || tag == "drum & bass")
        return "drum and bass";
    if (tag == "hip-hop" || tag == "hiphop")
        return "hip hop";
    if (tag == "80's" || tag == "1980s" || tag == "80er")
        return "80s";
    if (tag == "90's" || tag == "1990s" || tag == "90er")
        return "90s";
    if (tag == "pop music" || tag == "música pop" || tag == "pop en español e inglés")
        return "pop";

    return tag;
}

// Processes the raw JSON from the API into a clean, sorted list of genre tags.
std::vector<std::string> curate_tags(const json& raw_tags) {
    std::map<std::string, int> genre_counts;
    const std::set<std::string> blacklist = {
        "music",       "radio",         "fm",      "news", "talk",   "live",    "free",       "online",
        "hits",        "musica",        "noticias", "various", "misc", "entertainment", "am",
        "estación",    "méxico",    "norteamérica", "música", "pop rock", "latinoamérica",
        "español", "community radio", "local news", "música en español"};

    if (!raw_tags.is_array()) {
        throw std::runtime_error("Received invalid data from API; expected a JSON array.");
    }

    for (const auto& tag_obj : raw_tags) {
        if (!tag_obj.is_object() || !tag_obj.contains("name") || !tag_obj.contains("stationcount")) {
            continue;
        }

        std::string name = tag_obj["name"].get<std::string>();
        int count = tag_obj["stationcount"].get<int>();

        // Basic sanity checks
        if (name.length() < 3 || name.length() > 25)
            continue;
        if (!isalpha(name[0]))
            continue; // Must start with a letter

        std::string normalized = normalize_tag(name);

        if (blacklist.count(normalized))
            continue;

        genre_counts[normalized] += count;
    }

    std::vector<std::string> curated_list;
    for (const auto& pair : genre_counts) {
        if (pair.second >= 50) { // Keep only tags with a high cumulative station count
            curated_list.push_back(pair.first);
        }
    }

    std::sort(curated_list.begin(), curated_list.end());
    return curated_list;
}

void handle_list_tags() {
    std::cout << "Fetching available genres from Radio Browser API..." << std::endl;
    try {
        // We call the script from the build directory.
        std::string raw_json_str = exec_process("./build/api_helper.sh --list-tags");

        if (raw_json_str.empty() || raw_json_str.rfind("Error:", 0) == 0) {
            std::cerr << "Error: Failed to fetch data from the helper script." << std::endl;
            std::cerr << "Please check your internet connection and ensure 'curl' is installed." << std::endl;
            if(!raw_json_str.empty()) {
                std::cerr << "Script output: " << raw_json_str << std::endl;
            }
            return;
        }

        json raw_tags = json::parse(raw_json_str);
        std::vector<std::string> tags = curate_tags(raw_tags);

        std::cout << "\n--- Curated Radio Genres ---" << std::endl;
        for (const auto& tag : tags) {
            std::cout << tag << std::endl;
        }
        std::cout << "--------------------------\n" << std::endl;
        std::cout << "Use a tag with the --curate flag to create a new station list." << std::endl;
        std::cout << "Example: ./build/stream-hopper --curate techno" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\nAn error occurred while listing tags: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // --- Command-line mode dispatcher ---
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--list-tags") {
            handle_list_tags();
            return 0; // Exit after handling the utility command.
        }
    }

    // --- Default Player Mode ---
    suppress_stderr();

    try {
        PersistenceManager persistence;
        StationData station_data = persistence.loadStations();

        StationManager manager(station_data);
        RadioPlayer player(manager);

        player.run();

    } catch (const std::exception& e) {
        if (stdscr != NULL && !isendwin()) {
            endwin();
        }

        std::cout << "\n\nA critical error occurred during startup:\n" << e.what() << std::endl;
        std::cout << "The application must close." << std::endl;

        FILE* logfile = fopen("stream_hopper_crash.log", "a");
        if (logfile) {
            time_t now = time(0);
            char dt[30];
            strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", localtime(&now));
            fprintf(logfile, "[%s] Critical Error: %s\n", dt, e.what());
            fclose(logfile);
        }
        return 1;
    }

    return 0;
}
