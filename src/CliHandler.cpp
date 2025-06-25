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

// --- Helper Functions for Parsing API Data ---
namespace {
    std::vector<CuratorStation> parse_station_candidates(const json& stations_json) {
        std::vector<CuratorStation> candidates;
        if (!stations_json.is_array()) {
            return candidates;
        }
        for (const auto& entry : stations_json) {
            CuratorStation station;
            station.name = entry.value("name", "Unknown");
            if (entry.contains("url_resolved") && !entry.at("url_resolved").is_null()) {
                 station.urls.push_back(entry.at("url_resolved").get<std::string>());
            }
            station.votes = entry.value("votes", 0);

            if (!station.name.empty() && !station.urls.empty()) {
                candidates.push_back(station);
            }
        }
        return candidates;
    }

    std::string normalize_tag(std::string tag) {
        std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char c) { return std::tolower(c); });
        if (tag == "dnb" || tag == "drum and bass" || tag == "drum & bass") return "drum and bass";
        if (tag == "hip-hop" || tag == "hiphop") return "hip hop";
        if (tag == "80's" || tag == "1980s" || tag == "80er") return "80s";
        if (tag == "90's" || tag == "1990s" || tag == "90er") return "90s";
        if (tag == "pop music" || tag == "música pop" || tag == "pop en español e inglés") return "pop";
        return tag;
    }

    std::vector<std::string> curate_tags(const json& raw_tags) {
        const std::set<std::string> blacklist = {
            "aac", "mp3", "ogg", "flac", "wma", "streaming", "internet radio", "aac+", "online radio", "shoutcast",
            "icecast", "music", "radio", "fm", "news", "talk", "live", "free", "online", "hits", "musica", "noticias",
            "various", "misc", "entertainment", "am", "estación", "méxico", "norteamérica", "música", "pop rock",
            "latinoamérica", "español", "community radio", "local news", "música en español", "best", "top", "all",
            "hd", "web", "webradio", "abc", "quality", "1", "international", "world",
        };
        if (!raw_tags.is_array()) {
            return {};
        }
        
        std::vector<std::string> final_list;
        std::set<std::string> added_normalized_tags;

        for (const auto& tag_obj : raw_tags) {
             if (!tag_obj.is_object() || !tag_obj.contains("name") || !tag_obj.contains("stationcount")) continue;
            std::string name = tag_obj["name"].get<std::string>();
            int count = tag_obj["stationcount"].get<int>();

            if (count < 50) continue;
            if (name.length() < 3 || name.length() > 25) continue;

            std::string normalized = normalize_tag(name);
            if(blacklist.count(normalized)) continue;
            if(added_normalized_tags.count(normalized)) continue;

            final_list.push_back(name);
            added_normalized_tags.insert(normalized);
        }
        return final_list;
    }

    void suppress_stderr() {
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null == -1) return;
        dup2(dev_null, 2);
        close(dev_null);
    }
} // namespace

std::vector<std::string> CliHandler::get_curated_tags() {
    try {
        std::string path = "/json/tags?order=stationcount&reverse=true&hidebroken=true";
        std::string command = "./build/api_helper.sh '" + path + "'";
        std::string raw_json_str = exec_process(command.c_str());

        if (raw_json_str.empty() || raw_json_str.rfind("Error:", 0) == 0) {
            std::cerr << "Error fetching tags: " << raw_json_str << std::endl;
            return {};
        }
        json raw_tags = json::parse(raw_json_str);
        return curate_tags(raw_tags);
    } catch (const std::exception& e) {
        std::cerr << "\nAn error occurred while fetching tags: " << e.what() << std::endl;
        return {};
    }
}

void CliHandler::handle_list_tags() {
    std::cout << "Fetching available genres from Radio Browser API..." << std::endl;
    try {
        std::vector<std::string> tags = get_curated_tags();
        if (tags.empty()) {
            std::cout << "No suitable genres found after curation." << std::endl;
            return;
        }
        std::cout << "\n--- Available Radio Genres ---" << std::endl;
        for (const auto& tag : tags) {
            std::cout << tag << std::endl;
        }
        std::cout << "--------------------------\n" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nAn error occurred while listing tags: " << e.what() << std::endl;
    }
}

std::vector<CuratorStation> CliHandler::get_curation_candidates(const std::string& genre) {
    try {
        std::string encoded_genre = url_encode(genre, UrlEncodingStyle::PATH_PERCENT);
        std::string path = "/json/stations/bytag/" + encoded_genre + "?order=votes&reverse=true&hidebroken=true&limit=100";
        std::string command = "./build/api_helper.sh '" + path + "'";
        std::string stations_json_str = exec_process(command.c_str());

        if (stations_json_str.empty() || stations_json_str.rfind("Error:", 0) == 0) {
            std::cerr << "Error fetching stations for genre '" << genre << "': " << stations_json_str << std::endl;
            return {};
        }
        json stations_json = json::parse(stations_json_str);
        return parse_station_candidates(stations_json);
    } catch (const std::exception& e) {
        std::cerr << "\nAn error occurred while fetching stations for genre '" << genre << "': " << e.what() << std::endl;
        return {};
    }
}

void CliHandler::handle_curate_genre(const std::string& genre) {
    std::cout << "Fetching stations for genre: '" << genre << "'..." << std::endl;
    try {
        std::vector<CuratorStation> candidates = get_curation_candidates(genre);

        if (candidates.empty()) {
            std::cout << "No stations found for the genre '" << genre << "'." << std::endl;
            return;
        }
        std::cout << "Successfully fetched " << candidates.size() << " station candidates." << std::endl;

        suppress_stderr();
        CuratorApp app(genre, std::move(candidates));
        app.run();

        std::string genre_filename = genre + ".jsonc";
        std::cout << "\nCuration complete. Your curated list is in '" << genre_filename << "'." << std::endl;
        std::cout << "To use it, run: ./build/stream-hopper --from \"" << genre_filename << "\"" << std::endl;
    } catch (const std::exception& e) {
        if (stdscr != NULL && !isendwin()) {
            endwin();
        }
        std::cerr << "\nAn error occurred during curation: " << e.what() << std::endl;
    }
}
