// src/UIManager.cpp
#include "UIManager.h"
#include "RadioPlayer.h"
#include "RadioStream.h"
#include <ncurses.h>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <locale.h>
#include <iomanip>
#include <sstream>
#include "nlohmann/json.hpp"

#define COMPACT_MODE_WIDTH 80
#define DEFAULT_INPUT_TIMEOUT 100 // Default responsiveness

std::string truncate_string(const std::string& str, size_t width) {
    if (width > 3 && str.length() > width) {
        return str.substr(0, width - 3) + "...";
    }
    return str;
}

std::string format_history_timestamp(const std::string& ts_str) {
    std::tm tm = {};
    std::stringstream ss(ts_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        return ts_str.substr(0, 5);
    }

    auto entry_time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    auto now = std::chrono::system_clock::now();
    
    time_t t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::localtime(&t_now);
    tm_now.tm_hour = 0; tm_now.tm_min = 0; tm_now.tm_sec = 0;
    auto today_start = std::chrono::system_clock::from_time_t(std::mktime(&tm_now));

    if (entry_time >= today_start) {
        char buf[10];
        std::strftime(buf, sizeof(buf), "%H:%M", &tm);
        return std::string(buf);
    }

    auto yesterday_start = today_start - std::chrono::hours(24);
    if (entry_time >= yesterday_start) {
        return "Yesterday";
    }

    char buf[20];
    std::strftime(buf, sizeof(buf), "%b %d", &tm);
    return std::string(buf);
}


UIManager::UIManager() : m_station_scroll_offset(0) {
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(DEFAULT_INPUT_TIMEOUT); // Use the defined default
    start_color();
    use_default_colors();
    init_pair(1, COLOR_YELLOW, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_CYAN, -1);
    init_pair(4, COLOR_MAGENTA, -1); // Color for Copy Mode
}

UIManager::~UIManager() {
    if (stdscr != NULL && !isendwin()) {
        endwin();
    }
}

void UIManager::setInputTimeout(int milliseconds) {
    timeout(milliseconds);
}

// --- UPDATED draw IMPLEMENTATION ---
void UIManager::draw(const std::vector<RadioStream>& stations, int active_station_idx, 
                     const nlohmann::json& history, ActivePanel active_panel, int scroll_offset, bool is_small_mode,
                     int remaining_seconds, int total_duration, bool is_copy_mode) {
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
        draw_compact_mode(width, height, stations, active_station_idx, history, active_panel, scroll_offset, is_small_mode, remaining_seconds, total_duration, is_copy_mode);
        draw_footer_bar(height - 1, width, true, is_small_mode, is_copy_mode);
    } else {
        draw_full_mode(width, height, stations, active_station_idx, history, active_panel, scroll_offset, is_small_mode, remaining_seconds, total_duration, is_copy_mode);
        draw_footer_bar(height - 1, width, false, is_small_mode, is_copy_mode);
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
    std::string full_header = " STREAM HOPPER  |  LIVE  |  🔊 VOL: " + std::to_string((int)current_volume) + "%  |  🕐 " + time_str + " ";
    attron(A_REVERSE);
    mvprintw(0, 0, "%s", std::string(width, ' ').c_str());
    mvprintw(0, 1, "%s", truncate_string(full_header, width - 2).c_str());
    attroff(A_REVERSE);
}

// --- UPDATED draw_footer_bar IMPLEMENTATION ---
void UIManager::draw_footer_bar(int y, int width, bool is_compact, bool is_small_mode, bool is_copy_mode) {
    std::string footer_text;
    if (is_copy_mode) {
        footer_text = " [COPY MODE] UI Paused. Press any key to resume... ";
    } else if (is_small_mode) {
        footer_text = " [S] Exit Discovery   [C] Copy Mode   [Q] Quit ";
    } else if (is_compact) {
        footer_text = " [Nav]   [Tab] Panel   [F] Favorite   [C] Copy   [Q] Quit ";
    } else {
        footer_text = " [↑↓] Navigate   [↵] Mute   [⇥] Panel   [F] Fav   [C] Copy   [Q] Quit ";
    }
    
    attron(A_REVERSE);
    mvprintw(y, 0, "%*s", width, "");

    // --- ADDED: Special color for Copy Mode ---
    if (is_copy_mode) {
        attron(COLOR_PAIR(4));
        attron(A_BOLD);
    }
    
    if ((int)footer_text.length() < width) {
        mvprintw(y, (width - footer_text.length()) / 2, "%s", footer_text.c_str());
    } else {
        mvprintw(y, 1, "%s", truncate_string(footer_text, width - 2).c_str());
    }
    
    if (is_copy_mode) {
        attroff(A_BOLD);
        attroff(COLOR_PAIR(4));
    }
    attroff(A_REVERSE);
}

// --- UPDATED draw_compact_mode IMPLEMENTATION ---
void UIManager::draw_compact_mode(int width, int height, const std::vector<RadioStream>& stations, int active_idx,
                                  const nlohmann::json& history, ActivePanel active_panel, int scroll_offset, bool is_small_mode,
                                  int remaining_seconds, int total_duration, bool is_copy_mode) {
    if (stations.empty()) return;
    const RadioStream& active_station = stations[active_idx];

    int now_playing_h = is_small_mode ? 6 : 5;
    int remaining_h = height - now_playing_h - 2;
    int stations_h = std::max(3, static_cast<int>(remaining_h * 0.6));
    int history_h = remaining_h - stations_h;

    if (history_h < 3) {
        stations_h = remaining_h;
        history_h = 0;
    }

    int stations_y = 2 + now_playing_h;
    int history_y = stations_y + stations_h;

    draw_now_playing_panel(2, 1, width - 2, now_playing_h, active_station, is_small_mode, remaining_seconds, total_duration);

    draw_box(stations_y, 1, width - 2, stations_h, "STATIONS", active_panel == ActivePanel::STATIONS && !is_copy_mode);
    
    int visible_items = stations_h > 2 ? stations_h - 2 : 0;
    if (active_idx < m_station_scroll_offset) {
        m_station_scroll_offset = active_idx;
    }
    if (active_idx >= m_station_scroll_offset + visible_items) {
        m_station_scroll_offset = active_idx - visible_items + 1;
    }

    for (int i = 0; i < visible_items; ++i) {
        int station_idx = m_station_scroll_offset + i;
        if (station_idx >= (int)stations.size()) break;

        const auto& station = stations[station_idx];
        bool is_active = (station_idx == active_idx);
        
        if (is_active && !is_copy_mode) {
            attron(A_REVERSE);
        }

        std::string status_icon = "  ";
        if(is_active) {
            if (station.isBuffering()) status_icon = "🤔 ";
            else if (station.isMuted()) status_icon = "🔇 ";
            else status_icon = "▶️ ";
        }
        
        std::string fav_icon = station.isFavorite() ? "⭐ " : " ";
        std::string line = status_icon + fav_icon + station.getName();
        
        mvwprintw(stdscr, stations_y + 1 + i, 3, "%-*s", width - 5, truncate_string(line, width - 6).c_str());

        if (is_active && !is_copy_mode) {
            attroff(A_REVERSE);
        }
    }

    if (history_h > 0) {
        draw_history_panel(history_y, 1, width - 2, history_h, active_station, history, active_panel == ActivePanel::HISTORY && !is_copy_mode, scroll_offset);
    }
}

// --- UPDATED draw_full_mode IMPLEMENTATION ---
void UIManager::draw_full_mode(int width, int height, const std::vector<RadioStream>& stations, int active_station_idx, 
                               const nlohmann::json& history, ActivePanel active_panel, int scroll_offset, bool is_small_mode,
                               int remaining_seconds, int total_duration, bool is_copy_mode) {
    if (stations.empty()) return;

    int left_panel_w = std::max(35, width / 3);
    int right_panel_w = width - left_panel_w;
    int top_right_h = is_small_mode ? 7 : 6;
    int bottom_right_h = height - top_right_h - 2;

    draw_stations_panel(1, 0, left_panel_w, height - 2, stations, active_station_idx, active_panel == ActivePanel::STATIONS && !is_copy_mode);
    const RadioStream& current_station = stations[active_station_idx];
    
    draw_now_playing_panel(1, left_panel_w, right_panel_w, top_right_h, current_station, is_small_mode, remaining_seconds, total_duration);
    
    draw_history_panel(1 + top_right_h, left_panel_w, right_panel_w, bottom_right_h, current_station, history, active_panel == ActivePanel::HISTORY && !is_copy_mode, scroll_offset);
}

void UIManager::draw_stations_panel(int y, int x, int w, int h, const std::vector<RadioStream>& stations, 
                                    int active_station_idx, bool is_focused) {
    draw_box(y, x, w, h, "STATIONS", is_focused);
    int inner_w = w - 4;

    int visible_items = h > 2 ? h - 2 : 0;
    if (active_station_idx < m_station_scroll_offset) {
        m_station_scroll_offset = active_station_idx;
    }
    if (active_station_idx >= m_station_scroll_offset + visible_items) {
        m_station_scroll_offset = active_station_idx - visible_items + 1;
    }

    for (int i = 0; i < visible_items; ++i) {
        int station_idx = m_station_scroll_offset + i;
        if (station_idx >= (int)stations.size()) break;

        const auto& station = stations[station_idx];
        bool is_active = (station_idx == active_station_idx);
        
        if (is_active) {
            attron(A_REVERSE);
        }
        
        std::string status_icon;
        if (station.isBuffering()) {
            status_icon = "🤔 ";
        } else if (station.isMuted()) {
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

void UIManager::draw_now_playing_panel(int y, int x, int w, int h, const RadioStream& station, bool is_small_mode,
                                       int remaining_seconds, int total_duration) {
    std::string box_title = is_small_mode ? "🤖 DISCOVERY MODE" : "▶️  NOW PLAYING";
    draw_box(y, x, w, h, box_title, false);

    int inner_w = w - 4;
    
    std::string title = station.isBuffering() ? "Buffering..." : station.getCurrentTitle();
    attron(A_BOLD);
    mvwprintw(stdscr, y + 2, x + 3, "%s", truncate_string(title, inner_w - 2).c_str());
    attroff(A_BOLD);
    
    mvwprintw(stdscr, y + 3, x + 3, "%s", truncate_string(station.getName(), inner_w - 2).c_str());

    if (is_small_mode) {
        int bar_width = inner_w - 2;
        if (bar_width > 0) {
            double elapsed_percent = 0.0;
            if (total_duration > 0) {
                elapsed_percent = static_cast<double>(total_duration - remaining_seconds) / total_duration;
            }
            int filled_width = static_cast<int>(elapsed_percent * bar_width);

            mvwprintw(stdscr, y + h - 2, x + 2, "[");
            attron(COLOR_PAIR(2));
            for(int i = 0; i < filled_width; ++i) mvwaddch(stdscr, y + h - 2, x + 3 + i, ACS_BLOCK);
            attroff(COLOR_PAIR(2));
            for(int i = filled_width; i < bar_width; ++i) mvwaddch(stdscr, y + h - 2, x + 3 + i, '.');
            mvwprintw(stdscr, y + h - 2, x + 3 + bar_width, "]");
            
            std::string time_text = "Next in " + std::to_string(remaining_seconds) + "s";
            mvwprintw(stdscr, y + 1, x + w - time_text.length() - 2, "%s", time_text.c_str());
        }
    } else {
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
}

void UIManager::draw_history_panel(int y, int x, int w, int h, const RadioStream& station, 
                                   const nlohmann::json& history, bool is_focused, int scroll_offset) {
    if (h <= 0) return;
    draw_box(y, x, w, h, "📝 RECENT HISTORY", is_focused);
    int inner_w = w - 5;
    int panel_height = h - 2;

    const auto& station_name = station.getName();
    if (history.contains(station_name)) {
        const auto& station_history = history.at(station_name);
        if (station_history.empty()) return;

        int display_count = 0;
        auto start_it = station_history.rbegin();
        if (scroll_offset < (int)station_history.size()) {
            std::advance(start_it, scroll_offset);
        }

        for (auto it = start_it; it != station_history.rend() && display_count < panel_height; ++it, ++display_count) {
            const auto& entry = *it;
            if (entry.is_array() && entry.size() == 2) {
                std::string full_ts = entry[0].get<std::string>();
                std::string title_str = entry[1].get<std::string>();
                
                std::string time_str = format_history_timestamp(full_ts);
                
                std::stringstream line_ss;
                line_ss << std::setw(9) << std::left << time_str << "│ " << title_str;
                
                mvwprintw(stdscr, y + 1 + display_count, x + 3, "%s", truncate_string(line_ss.str(), inner_w).c_str());
            }
        }
    }
}

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
