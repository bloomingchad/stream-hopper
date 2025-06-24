#include "CliHandler.h"

#include <fcntl.h>
#include <ncurses.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include "CuratorApp.h"
#include "PersistenceManager.h"
#include "Utils.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace {
    // Helper functions are now local to this file.
    void suppress_stderr() {
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null == -1) {
            return;
        }
        dup2(dev_null, 2);
        close(dev_null);
    }

    std::string normalize_tag(std::string tag) {
        std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char c) { return std::tolower(c); });
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

    std::vector<std::string> curate_tags(const json& raw_tags) {
        std::map<std::string, int> genre_counts;
        const std::set<std::string> blacklist = {"music",
                                                 "radio",
                                                 "fm",
                                                 "news",
                                                 "talk",
                                                 "live",
                                                 "free",
                                                 "online",
                                                 "hits",
                                                 "musica",
                                                 "noticias",
                                                 "various",
                                                 "misc",
                                                 "entertainment",
                                                 "am",
                                                 "estación",
                                                 "méxico",
                                                 "norteamérica",
                                                 "música",
                                                 "pop rock",
                                                 "latinoamérica",
                                                 "español",
                                                 "community radio",
                                                 "local news",
                                                 "música en español"};
        if (!raw_tags.is_array()) {
            throw std::runtime_error("Received invalid data from API; expected a JSON array.");
        }
        for (const auto& tag_obj : raw_tags) {
            if (!tag_obj.is_object() || !tag_obj.contains("name") || !tag_obj.contains("stationcount")) {
                continue;
            }
            std::string name = tag_obj["name"].get<std::string>();
            int count = tag_obj["stationcount"].get<int>();
            if (name.length() < 3 || name.length() > 25)
                continue;
            if (!isalpha(name[0]))
                continue;
            std::string normalized = normalize_tag(name);
            if (blacklist.count(normalized))
                continue;
            genre_counts[normalized] += count;
        }
        std::vector<std::string> curated_list;
        for (const auto& pair : genre_counts) {
            if (pair.second >= 50) {
                curated_list.push_back(pair.first);
            }
        }
        std::sort(curated_list.begin(), curated_list.end());
        return curated_list;
    }
} // namespace

void CliHandler::handle_list_tags() {
    std::cout << "Fetching available genres from Radio Browser API..." << std::endl;
    try {
        std::string raw_json_str = exec_process("./build/api_helper.sh --list-tags");
        if (raw_json_str.empty() || raw_json_str.rfind("Error:", 0) == 0) {
            std::cerr << "Error: Failed to fetch data from the helper script." << std::endl;
            if (!raw_json_str.empty()) {
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

void CliHandler::handle_curate_genre(const std::string& genre) {
    std::cout << "Fetching stations for genre: '" << genre << "'..." << std::endl;
    try {
        std::string command = "./build/api_helper.sh --bygenre \"" + genre + "\"";
        std::string stations_json_str = exec_process(command.c_str());
        if (stations_json_str.empty() || stations_json_str.rfind("Error:", 0) == 0) {
            std::cerr << "Error: Failed to fetch stations from the helper script." << std::endl;
            if (!stations_json_str.empty()) {
                std::cerr << "Script output: " << stations_json_str << std::endl;
            }
            return;
        }
        json stations_json = json::parse(stations_json_str);
        if (!stations_json.is_array() || stations_json.empty()) {
            std::cout << "No working stations found for the genre '" << genre << "'." << std::endl;
            return;
        }
        std::string genre_filename = genre + ".jsonc";
        std::ofstream o(genre_filename);
        o << std::setw(4) << stations_json << std::endl;
        o.close();

        std::cout << "Successfully fetched " << stations_json.size() << " station candidates." << std::endl;

        PersistenceManager persistence;
        // <<< FIX: Correctly call the new method
        std::vector<CuratorStation> candidates = persistence.loadCurationCandidates(genre_filename);

        suppress_stderr();
        CuratorApp app(genre, std::move(candidates));
        app.run();

        std::cout << "\nCuration complete. Your curated list is in '" << genre_filename << "'." << std::endl;
        std::cout << "To use it, run: ./build/stream-hopper --from " << genre_filename << std::endl;
    } catch (const std::exception& e) {
        if (stdscr != NULL && !isendwin()) {
            endwin();
        }
        std::cerr << "\nAn error occurred during curation: " << e.what() << std::endl;
    }
}
