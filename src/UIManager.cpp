// src/UIManager.cpp
#include "UIManager.h"
#include "RadioStream.h"
#include <ncurses.h>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <algorithm> // for std::max
#include "nlohmann/json.hpp"

// Helper function to truncate strings that are too long for the panel
std::string truncate_string(const std::string& str, size_t width) {
    if (str.length() > width) {
        return str.substr(0, width - 3) + "...";
    }
    return str;
}


UIManager::UIManager() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_YELLOW, -1);
    init_pair(2, COLOR_GREEN, -1);
}

UIManager::~UIManager() {
    if (stdscr != NULL && !isendwin()) {
        endwin();
    }
}

void UIManager::draw(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active, const nlohmann::json& history) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    // Get volume of the currently playing (or fading in) station
    double display_vol = 0.0;
    for(const auto& s : stations) {
        if(s.getCurrentVolume() > 0) {
            display_vol = s.getCurrentVolume();
            break;
        }
    }
    if(display_vol == 0.0 && !stations.empty()) {
        display_vol = stations[active_station_idx].getTargetVolume();
    }


    draw_header_bar(width, display_vol);
    draw_full_mode(width, height, stations, active_station_idx, history);
    draw_footer_bar(height - 1, width);
    refresh();
}

int UIManager::getInput() {
    return getch();
}

void UIManager::draw_header_bar(int width, double current_volume) {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char time_str[10];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", ltm);
    
    attron(A_REVERSE);
    mvprintw(0, 0, "%s", std::string(width, ' ').c_str());
    mvprintw(0, 1, " 📻 STREAM HOPPER | 🟢 LIVE | 🔊 VOL: %.0f%% | 🕒 %s UTC ", current_volume, time_str);
    attroff(A_REVERSE);
}

void UIManager::draw_footer_bar(int y, int width) {
    std::string footer_text = " [↑↓] Navigate   [▶] Play/Mute   [⭐] Favorite   [Q] Quit ";
    attron(A_REVERSE);
    mvprintw(y, 0, "%*s", width, "");
    mvprintw(y, (width - footer_text.length()) / 2, "%s", footer_text.c_str());
    attroff(A_REVERSE);
}

void UIManager::draw_full_mode(int width, int height, const std::vector<RadioStream>& stations, int active_station_idx, const nlohmann::json& history) {
    if (stations.empty()) return;

    int left_panel_w = std::max(30, width / 3);
    int right_panel_w = width - left_panel_w;
    int top_right_h = 7;
    int bottom_right_h = height - top_right_h - 2;

    draw_stations_panel(1, 0, left_panel_w, height - 2, stations, active_station_idx);
    
    const RadioStream& current_station = stations[active_station_idx];
    draw_now_playing_panel(1, left_panel_w, right_panel_w, top_right_h, current_station);
    draw_history_panel(1 + top_right_h, left_panel_w, right_panel_w, bottom_right_h, current_station, history);
}

void UIManager::draw_stations_panel(int y, int x, int w, int h, const std::vector<RadioStream>& stations, int active_station_idx) {
    draw_box(y, x, w, h, "STATIONS");
    int inner_w = w - 4;

    for (int i = 0; i < h - 2 && i < (int)stations.size(); ++i) {
        const auto& station = stations[i];
        bool is_active = (i == active_station_idx);
        
        if (is_active) {
            attron(A_REVERSE);
        }

        std::string status_icon = "  "; // Default silent
        if (station.isMuted()) status_icon = "🔇";
        else if (station.getCurrentVolume() > 0) status_icon = "▶️ ";
        
        std::string line = status_icon + " " + station.getName();
        mvprintw(y + 1 + i, x + 2, "%-*s", inner_w, truncate_string(line, inner_w).c_str());

        if (is_active) {
            attroff(A_REVERSE);
        }
    }
}

void UIManager::draw_now_playing_panel(int y, int x, int w, int h, const RadioStream& station) {
    draw_box(y, x, w, h, "NOW PLAYING");
    int inner_w = w - 4;

    // Display title
    std::string title = station.getCurrentTitle();
    mvwprintw(stdscr, y + 2, x + 3, "%s", truncate_string(title, inner_w - 2).c_str());
    
    // Display station name again for clarity
    attron(A_BOLD);
    mvwprintw(stdscr, y + 4, x + 3, "%s", truncate_string(station.getName(), inner_w - 2).c_str());
    attroff(A_BOLD);

    // Display Volume bar
    int bar_width = inner_w - 12; // " 🔊 [ ... ] "
    double vol_percent = station.getCurrentVolume() / 100.0;
    int filled_width = static_cast<int>(vol_percent * bar_width);
    
    mvwprintw(stdscr, y + 1, x + 3, "🔊");
    for(int i = 0; i < bar_width; ++i) {
        if (i < filled_width) {
            attron(COLOR_PAIR(2));
            mvwaddch(stdscr, y + 1, x + 5 + i, ACS_BLOCK);
            attroff(COLOR_PAIR(2));
        } else {
            mvwaddch(stdscr, y + 1, x + 5 + i, ACS_CKBOARD);
        }
    }
    mvwprintw(stdscr, y + 1, x + 5 + bar_width + 1, "%.0f%%", station.getCurrentVolume());
}

void UIManager::draw_history_panel(int y, int x, int w, int h, const RadioStream& station, const nlohmann::json& history) {
    draw_box(y, x, w, h, "RECENT HISTORY");
    int inner_w = w - 4;

    const auto& station_name = station.getName();
    if (history.contains(station_name)) {
        const auto& station_history = history.at(station_name);
        int display_count = 0;
        // Iterate backwards to show most recent first
        for (auto it = station_history.rbegin(); it != station_history.rend() && display_count < h - 2; ++it, ++display_count) {
            const auto& entry = *it;
            if (entry.is_array() && entry.size() == 2) {
                // We expect ["timestamp", "title"]
                std::string time_str = entry[0].get<std::string>();
                std::string title_str = entry[1].get<std::string>();
                
                // Extract just HH:MM from full timestamp if needed
                if(time_str.length() > 5) {
                    size_t pos = time_str.find(" ");
                    if(pos != std::string::npos && pos + 6 <= time_str.length()) {
                       time_str = time_str.substr(pos + 1, 5);
                    } else if (time_str.length() >= 5) {
                       time_str = time_str.substr(0, 5); // Fallback
                    }
                }

                std::string line = time_str + " | " + title_str;
                mvwprintw(stdscr, y + 1 + display_count, x + 2, "%s", truncate_string(line, inner_w).c_str());
            }
        }
    }
}


void UIManager::draw_box(int y, int x, int w, int h, const std::string& title) {
    mvwhline(stdscr, y, x + 1, ACS_HLINE, w - 2);
    mvwhline(stdscr, y + h - 1, x + 1, ACS_HLINE, w - 2);
    mvwvline(stdscr, y + 1, x, ACS_VLINE, h - 2);
    mvwvline(stdscr, y + 1, x + w - 1, ACS_VLINE, h - 2);
    mvwaddch(stdscr, y, x, ACS_ULCORNER);
    mvwaddch(stdscr, y, x + w - 1, ACS_URCORNER);
    mvwaddch(stdscr, y + h - 1, x, ACS_LLCORNER);
    mvwaddch(stdscr, y + h - 1, x + w - 1, ACS_LRCORNER);
    if (!title.empty()) {
        mvwprintw(stdscr, y, x + 3, " %s ", title.c_str());
    }
}
