// src/UIManager.cpp
#include "UIManager.h"
#include "RadioStream.h"
#include <ncurses.h>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <locale.h>
#include "nlohmann/json.hpp"

#define COMPACT_MODE_WIDTH 80

std::string truncate_string(const std::string& str, size_t width) {
    if (width > 3 && str.length() > width) {
        return str.substr(0, width - 3) + "...";
    }
    return str;
}

UIManager::UIManager() {
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
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
    init_pair(3, COLOR_CYAN, -1);
}

UIManager::~UIManager() {
    if (stdscr != NULL && !isendwin()) {
        endwin();
    }
}

void UIManager::draw(const std::vector<RadioStream>& stations, int active_station_idx, 
                     const nlohmann::json& history, ActivePanel active_panel, int scroll_offset) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    double display_vol = 0.0;
    bool is_small_mode = false;
    if (!stations.empty()) {
        const auto& active_station = stations[active_station_idx];
        display_vol = active_station.isMuted() ? 0.0 : active_station.getCurrentVolume();
    }
    
    draw_header_bar(width, display_vol);

    if (width < COMPACT_MODE_WIDTH || is_small_mode) {
        draw_compact_mode(width, height, stations, active_station_idx);
        draw_footer_bar(height - 1, width, true, is_small_mode);
    } else {
        draw_full_mode(width, height, stations, active_station_idx, history, active_panel, scroll_offset);
        draw_footer_bar(height - 1, width, false, is_small_mode);
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
    std::string full_header = " STREAM HOPPER  |  LIVE  |  VOL: " + std::to_string((int)current_volume) + "%  |  " + time_str + " UTC ";
    attron(A_REVERSE);
    mvprintw(0, 0, "%s", std::string(width, ' ').c_str());
    mvprintw(0, 1, "%s", truncate_string(full_header, width - 2).c_str());
    attroff(A_REVERSE);
}

void UIManager::draw_footer_bar(int y, int width, bool is_compact, bool is_small_mode) {
    std::string footer_text;
    if (is_small_mode) {
        footer_text = " [S] Exit Small Mode   [Q] Quit ";
    } else if(is_compact) {
        footer_text = " [↑↓] Move [↲] Play [F] Fav [S] Full [Q] Quit ";
    } else {
        footer_text = " [↑↓] Navigate   [↲] Play/Mute   [Tab] Switch Panel   [F] Favorite   [Q] Quit ";
    }
    
    attron(A_REVERSE);
    mvprintw(y, 0, "%*s", width, "");
    if ((int)footer_text.length() < width) {
        mvprintw(y, (width - footer_text.length()) / 2, "%s", footer_text.c_str());
    } else {
        mvprintw(y, 1, " [Q] Quit ");
    }
    attroff(A_REVERSE);
}

void UIManager::draw_compact_mode(int width, int height, const std::vector<RadioStream>& stations, int active_idx) {
    if (stations.empty()) return;
    
    const RadioStream& active_station = stations[active_idx];
    
    int box_h = 6;
    draw_box(2, 1, width - 2, box_h, "NOW PLAYING", false);

    std::string fav_star = active_station.isFavorite() ? "⭐ " : "";
    mvwprintw(stdscr, 3, 3, "%s", truncate_string(fav_star + active_station.getName(), width - 6).c_str());
    mvwprintw(stdscr, 4, 5, "%s", truncate_string(active_station.getCurrentTitle(), width - 8).c_str());
    
    int list_start_y = 2 + box_h;
    int drawn_count = 0;
    for (size_t i = 0; i < stations.size() && drawn_count < height - list_start_y - 1; ++i) {
        if ((int)i == active_idx) continue;
        
        const auto& station = stations[i];
        std::string line = "  " + station.getName();
        if (station.isFavorite()) {
            line = "⭐" + line;
        } else {
            line = " " + line;
        }
        mvwprintw(stdscr, list_start_y + drawn_count, 2, "%s", truncate_string(line, width - 5).c_str());
        drawn_count++;
    }
}

void UIManager::draw_full_mode(int width, int height, const std::vector<RadioStream>& stations, int active_station_idx, 
                               const nlohmann::json& history, ActivePanel active_panel, int scroll_offset) {
    if (stations.empty()) return;

    int left_panel_w = std::max(35, width / 3);
    int right_panel_w = width - left_panel_w;
    int top_right_h = 6;
    int bottom_right_h = height - top_right_h - 2;

    draw_stations_panel(1, 0, left_panel_w, height - 2, stations, active_station_idx, active_panel == ActivePanel::STATIONS);
    const RadioStream& current_station = stations[active_station_idx];
    draw_now_playing_panel(1, left_panel_w, right_panel_w, top_right_h, current_station);
    draw_history_panel(1 + top_right_h, left_panel_w, right_panel_w, bottom_right_h, current_station, history, active_panel == ActivePanel::HISTORY, scroll_offset);
}

void UIManager::draw_stations_panel(int y, int x, int w, int h, const std::vector<RadioStream>& stations, 
                                    int active_station_idx, bool is_focused) {
    draw_box(y, x, w, h, "STATIONS", is_focused);
    int inner_w = w - 4;

    for (int i = 0; i < h - 2 && i < (int)stations.size(); ++i) {
        const auto& station = stations[i];
        bool is_active = (i == active_station_idx);
        
        if (is_active) {
            attron(A_REVERSE);
        }

        std::string status_icon;
        if (station.isMuted()) {
            status_icon = "🔇 ";
        } else if (station.getCurrentVolume() > 0) {
            status_icon = "▶️ ";
        } else {
            status_icon = "   ";
        }
        
        std::string fav_icon = station.isFavorite() ? "⭐ " : "  ";
        
        std::string line = status_icon + fav_icon + station.getName();
        mvwprintw(stdscr, y + 1 + i, x + 2, "%-*s", inner_w + 1, truncate_string(line, inner_w).c_str());

        if (is_active) {
            attroff(A_REVERSE);
        }
    }
}

void UIManager::draw_now_playing_panel(int y, int x, int w, int h, const RadioStream& station) {
    draw_box(y, x, w, h, "NOW PLAYING", false);
    int inner_w = w - 4;

    std::string title = station.getCurrentTitle();
    attron(A_BOLD);
    mvwprintw(stdscr, y + 2, x + 3, "%s", truncate_string(title, inner_w - 2).c_str());
    attroff(A_BOLD);
    
    mvwprintw(stdscr, y + 3, x + 3, "%s", truncate_string(station.getName(), inner_w - 2).c_str());

    int bar_width = inner_w - 12; 
    if (bar_width > 0) {
        double vol_percent = station.isMuted() ? 0.0 : station.getCurrentVolume() / 100.0;
        int filled_width = static_cast<int>(vol_percent * bar_width);
        
        mvwprintw(stdscr, y + 1, x + 3, "🔊 [");
        attron(COLOR_PAIR(2));
        for(int i = 0; i < filled_width; ++i) mvwaddch(stdscr, y + 1, x + 6 + i, ACS_BLOCK);
        attroff(COLOR_PAIR(2));
        for(int i = filled_width; i < bar_width; ++i) mvwaddch(stdscr, y + 1, x + 6 + i, ACS_CKBOARD);
        mvwprintw(stdscr, y + 1, x + 6 + bar_width, "]");
        mvwprintw(stdscr, y + 1, x + 8 + bar_width, "%.0f%%", station.isMuted() ? 0.0 : station.getCurrentVolume());
    }
}

// REWRITTEN: This function now implements scrolling.
void UIManager::draw_history_panel(int y, int x, int w, int h, const RadioStream& station, 
                                   const nlohmann::json& history, bool is_focused, int scroll_offset) {
    draw_box(y, x, w, h, "RECENT HISTORY", is_focused);
    int inner_w = w - 5;
    int panel_height = h - 2;

    const auto& station_name = station.getName();
    if (history.contains(station_name)) {
        const auto& station_history = history.at(station_name);
        if (station_history.empty()) return;

        // Draw the visible part of the history list
        int display_count = 0;
        auto start_it = station_history.rbegin();
        if (scroll_offset < (int)station_history.size()) {
            std::advance(start_it, scroll_offset);
        }

        for (auto it = start_it; it != station_history.rend() && display_count < panel_height; ++it, ++display_count) {
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

// REWRITTEN: This function now colors the title text when focused.
void UIManager::draw_box(int y, int x, int w, int h, const std::string& title, bool is_focused) {
    if (is_focused) {
        attron(COLOR_PAIR(3));
    }

    mvwhline(stdscr, y, x + 1, ACS_HLINE, w - 2);
    mvwhline(stdscr, y + h - 1, x + 1, ACS_HLINE, w - 2);
    mvwvline(stdscr, y + 1, x, ACS_VLINE, h - 2);
    mvwvline(stdscr, y + 1, x + w - 1, ACS_VLINE, h - 2);
    mvwaddch(stdscr, y, x, ACS_ULCORNER);
    mvwaddch(stdscr, y, x + w - 1, ACS_URCORNER);
    mvwaddch(stdscr, y + h - 1, x, ACS_LLCORNER);
    mvwaddch(stdscr, y + h - 1, x + w - 1, ACS_LRCORNER);
    
    if (!title.empty()) {
        if (is_focused) {
            attron(A_BOLD);
        }
        mvwprintw(stdscr, y, x + 3, " %s ", title.c_str());
        if (is_focused) {
            attroff(A_BOLD);
        }
    }

    if (is_focused) {
        attroff(COLOR_PAIR(3));
    }
}
