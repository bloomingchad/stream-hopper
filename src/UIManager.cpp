// src/UIManager.cpp
#include "UIManager.h"
#include "RadioStream.h"
#include <ncurses.h>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <algorithm>
#include "nlohmann/json.hpp"

// Define a threshold for switching to compact mode
#define COMPACT_MODE_WIDTH 80

// Helper function to truncate strings
std::string truncate_string(const std::string& str, size_t width) {
    if (width > 3 && str.length() > width) {
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

// Main draw function now acts as a dispatcher based on terminal width
void UIManager::draw(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active, const nlohmann::json& history) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    double display_vol = 0.0;
    if (!stations.empty()) {
        const auto& active_station = stations[active_station_idx];
        display_vol = active_station.isMuted() ? 0.0 : active_station.getCurrentVolume();
    }
    
    draw_header_bar(width, display_vol);

    if (width < COMPACT_MODE_WIDTH) {
        draw_compact_mode(width, height, stations, active_station_idx);
        draw_footer_bar(height - 1, width, true);
    } else {
        draw_full_mode(width, height, stations, active_station_idx, history);
        draw_footer_bar(height - 1, width, false);
    }
    
    refresh();
}

int UIManager::getInput() {
    return getch();
}

void UIManager::draw_header_bar(int width, double current_volume) {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char time_str[10];
    strftime(time_str, sizeof(time_str), "%H:%M", ltm);

    std::string line1 = "STREAM HOPPER";
    std::string line2 = "LIVE";
    std::string line3 = "VOL: " + std::to_string((int)current_volume) + "%";
    std::string line4 = std::string(time_str) + " UTC";
    
    std::string full_header = " " + line1 + "  |  " + line2 + "  |  " + line3 + "  |  " + line4 + " ";

    attron(A_REVERSE);
    mvprintw(0, 0, "%s", std::string(width, ' ').c_str());
    if((int)full_header.length() < width) {
       mvprintw(0, 1, full_header.c_str());
    } else {
       mvprintw(0, 1, " Hopper ");
    }
    attroff(A_REVERSE);
}


void UIManager::draw_footer_bar(int y, int width, bool is_compact) {
    std::string footer_text;
    if(is_compact) {
        footer_text = " [↑↓] Move [↲] Play [Q] Quit ";
    } else {
        footer_text = " [↑↓] Navigate   [↲] Play/Mute   [S] Small Mode   [Q] Quit ";
    }
    
    attron(A_REVERSE);
    mvprintw(y, 0, "%*s", width, ""); // Clear line
    if ((int)footer_text.length() < width) {
        mvprintw(y, (width - footer_text.length()) / 2, "%s", footer_text.c_str());
    }
    attroff(A_REVERSE);
}

// NEW: Implementation for the compact layout
void UIManager::draw_compact_mode(int width, int height, const std::vector<RadioStream>& stations, int active_station_idx) {
    if (stations.empty()) return;
    
    const RadioStream& active_station = stations[active_station_idx];
    
    // Draw "Now Playing" box
    int box_h = 6;
    draw_box(2, 1, width - 2, box_h, "NOW PLAYING");

    // --- Content for Now Playing box ---
    // Title
    mvwprintw(stdscr, 4, 3, "%s", truncate_string(active_station.getCurrentTitle(), width - 6).c_str());
    
    // Volume Bar
    int bar_width = width - 15; // Space for icon and percentage
    if (bar_width > 0) {
        double vol_percent = active_station.getCurrentVolume() / 100.0;
        int filled_width = static_cast<int>(vol_percent * bar_width);
        
        mvwprintw(stdscr, 3, 3, "🔊 [");
        for(int i = 0; i < bar_width; ++i) {
            if (i < filled_width) {
                attron(COLOR_PAIR(2));
                mvwaddch(stdscr, 3, 6 + i, ACS_BLOCK);
                attroff(COLOR_PAIR(2));
            } else {
                mvwaddch(stdscr, 3, 6 + i, ' ');
            }
        }
        mvwprintw(stdscr, 3, 6 + bar_width, "]");
        mvwprintw(stdscr, 3, 6 + bar_width + 2, "%.0f%%", active_station.getCurrentVolume());
    }
    
    // --- Draw the rest of the station list ---
    int list_start_y = 2 + box_h + 1;
    for (int i = 0; i < height - list_start_y - 1 && i < (int)stations.size(); ++i) {
        const auto& station = stations[i];
        bool is_active = (i == active_station_idx);
        
        if (is_active) continue; // Don't draw the active station again
        
        std::string line = "   " + station.getName();
        mvwprintw(stdscr, list_start_y + i, 3, "%s", truncate_string(line, width - 6).c_str());
    }
}

// --- FULL MODE FUNCTIONS (UNCHANGED) ---

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
        mvwprintw(stdscr, y + 1 + i, x + 2, "%-*s", inner_w + 1, truncate_string(line, inner_w).c_str());

        if (is_active) {
            attroff(A_REVERSE);
        }
    }
}

void UIManager::draw_now_playing_panel(int y, int x, int w, int h, const RadioStream& station) {
    draw_box(y, x, w, h, "NOW PLAYING");
    int inner_w = w - 4;

    std::string title = station.getCurrentTitle();
    attron(A_BOLD);
    mvwprintw(stdscr, y + 2, x + 3, "%s", truncate_string(title, inner_w - 2).c_str());
    attroff(A_BOLD);
    
    mvwprintw(stdscr, y + 3, x + 3, "%s", truncate_string(station.getName(), inner_w - 2).c_str());

    int bar_width = inner_w - 12; 
    if (bar_width > 0) {
        double vol_percent = station.getCurrentVolume() / 100.0;
        int filled_width = static_cast<int>(vol_percent * bar_width);
        
        mvwprintw(stdscr, y + 5, x + 3, "🔊 [");
        attron(COLOR_PAIR(2));
        for(int i = 0; i < filled_width; ++i) mvwaddch(stdscr, y + 5, x + 6 + i, ACS_BLOCK);
        attroff(COLOR_PAIR(2));
        for(int i = filled_width; i < bar_width; ++i) mvwaddch(stdscr, y + 5, x + 6 + i, ' ');
        mvwprintw(stdscr, y + 5, x + 6 + bar_width, "]");
        mvwprintw(stdscr, y + 5, x + 8 + bar_width, "%.0f%%", station.getCurrentVolume());
    }
}

void UIManager::draw_history_panel(int y, int x, int w, int h, const RadioStream& station, const nlohmann::json& history) {
    draw_box(y, x, w, h, "RECENT HISTORY");
    int inner_w = w - 5;

    const auto& station_name = station.getName();
    if (history.contains(station_name)) {
        const auto& station_history = history.at(station_name);
        int display_count = 0;
        for (auto it = station_history.rbegin(); it != station_history.rend() && display_count < h - 2; ++it, ++display_count) {
            const auto& entry = *it;
            if (entry.is_array() && entry.size() == 2) {
                std::string time_str = entry[0].get<std::string>();
                std::string title_str = entry[1].get<std::string>();
                
                if(time_str.length() > 5) {
                    size_t pos = time_str.find(" ");
                    if(pos != std::string::npos && pos + 6 <= time_str.length()) {
                       time_str = time_str.substr(pos + 1, 5);
                    }
                }

                std::string line = time_str + " │ " + title_str;
                mvwprintw(stdscr, y + 1 + display_count, x + 3, "%s", truncate_string(line, inner_w).c_str());
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
