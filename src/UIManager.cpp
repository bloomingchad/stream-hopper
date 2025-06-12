// UIManager.cpp
#include "UIManager.h"
#include <ncurses.h>

UIManager::UIManager() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100);
}

UIManager::~UIManager() {
    if (stdscr != NULL && !isendwin()) {
        endwin();
    }
}

void UIManager::draw(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active, int remaining_seconds) {
    clear();
    draw_header(small_mode_active, remaining_seconds);
    if (!stations.empty()) {
        draw_station_list(stations, active_station_idx, small_mode_active);
        draw_footer(small_mode_active);
    }
    refresh();
}

int UIManager::getInput() {
    return getch();
}

void UIManager::draw_header(bool small_mode_active, int remaining_seconds) {
    if (small_mode_active) {
        mvprintw(0, 0, "Radio Switcher - SMALL MODE ACTIVE | S: Exit Small Mode | Q: Quit");
        mvprintw(1, 0, "Auto-rotating through all stations. Time left for current: %d seconds", remaining_seconds);
    } else {
        mvprintw(0, 0, "Radio Switcher (Refactored) | Q: Quit | Enter: Mute/Unmute | S: Small Mode");
    }
}

void UIManager::draw_station_list(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active) {
    for (size_t i = 0; i < stations.size(); ++i) {
        const auto& station = stations[i];
        bool is_active = (static_cast<int>(i) == active_station_idx);
        if (is_active) {
            attron(A_REVERSE);
        }
        std::string status = station.getStatusString(is_active, small_mode_active);
        mvprintw(2 + i, 2, "[%s] %s: %s (Vol: %.0f)",
                 status.c_str(),
                 station.getName().c_str(),
                 station.getCurrentTitle().c_str(),
                 station.getCurrentVolume());
        if (is_active) {
            attroff(A_REVERSE);
        }
    }
}

void UIManager::draw_footer(bool small_mode_active) {
    if (!small_mode_active) {
        mvprintw(LINES - 2, 0, "Use UP/DOWN arrows to switch stations.");
    } else {
        mvprintw(LINES - 2, 0, "Small Mode: Discovering radio stations automatically...");
    }
}
