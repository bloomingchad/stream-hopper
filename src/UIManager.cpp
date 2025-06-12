// src/UIManager.cpp
#include "UIManager.h"
#include "RadioStream.h"
#include <ncurses.h>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include "nlohmann/json.hpp"

UIManager::UIManager() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100); // Non-blocking input
    start_color();
    use_default_colors();
    // Initialize color pairs if needed later
    init_pair(1, COLOR_YELLOW, -1); // Example: Yellow text
}

UIManager::~UIManager() {
    if (stdscr != NULL && !isendwin()) {
        endwin();
    }
}

// Main draw function updated to call the full mode layout
void UIManager::draw(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active, const nlohmann::json& history) {
    clear();
    
    int height, width;
    getmaxyx(stdscr, height, width);

    draw_header_bar(width);
    
    // For now, we only draw the full mode. We'll add compact mode later.
    draw_full_mode(width, height, stations, active_station_idx, history);

    draw_footer_bar(height - 1, width);

    refresh();
}

int UIManager::getInput() {
    return getch();
}

void UIManager::draw_header_bar(int width) {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char time_str[10];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", ltm);
    
    std::string title = " STREAM HOPPER ";
    std::string status = "LIVE";
    std::string vol = "VOL: 100%";
    std::string time = std::string(time_str) + " UTC";

    attron(A_REVERSE);
    mvprintw(0, 0, "%s", std::string(width, ' ').c_str());
    mvprintw(0, 1, "%s| %s | %s | %s", title.c_str(), status.c_str(), vol.c_str(), time.c_str());
    attroff(A_REVERSE);
}

void UIManager::draw_footer_bar(int y, int width) {
    // This could be expanded with more controls
    std::string footer_text = " [↑↓] Navigate   [Enter] Play/Mute   [⭐] Favorite   [Q] Quit ";
    attron(A_REVERSE);
    mvprintw(y, 0, "%*s", width, ""); // Clear line
    mvprintw(y, 1, footer_text.c_str());
    attroff(A_REVERSE);
}

void UIManager::draw_full_mode(int width, int height, const std::vector<RadioStream>& stations, int active_station_idx, const nlohmann::json& history) {
    if (stations.empty()) return;

    // Define layout properties
    int left_panel_w = width / 3;
    int right_panel_w = width - left_panel_w;
    int top_right_h = 7; // Fixed height for "Now Playing"
    int bottom_right_h = height - top_right_h - 2; // -2 for header/footer

    // Draw Panels
    draw_stations_panel(1, 0, left_panel_w, height - 2, stations, active_station_idx);
    
    const RadioStream& current_station = stations[active_station_idx];
    draw_now_playing_panel(1, left_panel_w, right_panel_w, top_right_h, current_station);
    draw_history_panel(1 + top_right_h, left_panel_w, right_panel_w, bottom_right_h, current_station, history);
}

void UIManager::draw_stations_panel(int y, int x, int w, int h, const std::vector<RadioStream>& stations, int active_station_idx) {
    draw_box(y, x, w, h, "STATIONS");
    // Placeholder content
    mvwprintw(stdscr, y + 1, x + 2, "Station list will go here...");
}

void UIManager::draw_now_playing_panel(int y, int x, int w, int h, const RadioStream& station) {
    draw_box(y, x, w, h, "NOW PLAYING");
    // Placeholder content
    mvwprintw(stdscr, y + 1, x + 2, "Current song info will go here...");
}

void UIManager::draw_history_panel(int y, int x, int w, int h, const RadioStream& station, const nlohmann::json& history) {
    draw_box(y, x, w, h, "RECENT HISTORY");
    // Placeholder content
    mvwprintw(stdscr, y + 1, x + 2, "Song history will go here...");
}

// Helper to draw a box with a title
void UIManager::draw_box(int y, int x, int w, int h, const std::string& title) {
    // Draw borders
    mvwhline(stdscr, y, x + 1, ACS_HLINE, w - 2);
    mvwhline(stdscr, y + h - 1, x + 1, ACS_HLINE, w - 2);
    mvwvline(stdscr, y + 1, x, ACS_VLINE, h - 2);
    mvwvline(stdscr, y + 1, x + w - 1, ACS_VLINE, h - 2);

    // Draw corners
    mvwaddch(stdscr, y, x, ACS_ULCORNER);
    mvwaddch(stdscr, y, x + w - 1, ACS_URCORNER);
    mvwaddch(stdscr, y + h - 1, x, ACS_LLCORNER);
    mvwaddch(stdscr, y + h - 1, x + w - 1, ACS_LRCORNER);
    
    // Draw the title on the top border
    if (!title.empty()) {
        mvwprintw(stdscr, y, x + 3, "[ %s ]", title.c_str());
    }
}
