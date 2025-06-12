// src/UIManager.cpp
#include "UIManager.h"
#include "RadioStream.h" // Now include the full header here
#include <ncurses.h>
#include <string> // Required for std::string

UIManager::UIManager() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100); // Non-blocking input
    start_color(); // Enable color if needed later
    use_default_colors();
}

UIManager::~UIManager() {
    if (stdscr != NULL && !isendwin()) {
        endwin();
    }
}

// Main draw function is now simplified for testing the box.
void UIManager::draw(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active) {
    clear();
    
    int width, height;
    getmaxyx(stdscr, height, width);

    draw_header();

    // --- TEST DRAW ---
    // Draw a single box to test our new function.
    // In the next step, we'll replace this with the real layout.
    draw_box(2, 0, width, height - 4, "TEST BOX");
    mvprintw(3, 2, "This is a test of the box drawing function.");
    mvprintw(4, 2, "Press 'q' to quit.");
    // --- END TEST ---

    refresh();
}

int UIManager::getInput() {
    return getch();
}

void UIManager::draw_header() {
    mvprintw(0, 0, "Stream Hopper UI - Step 2");
}

// New implementation for drawing a titled box
void UIManager::draw_box(int y, int x, int w, int h, const std::string& title) {
    // Draw corners
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);

    // Draw horizontal lines
    for (int i = 1; i < w - 1; ++i) {
        mvaddch(y, x + i, ACS_HLINE);
        mvaddch(y + h - 1, x + i, ACS_HLINE);
    }

    // Draw vertical lines
    for (int i = 1; i < h - 1; ++i) {
        mvaddch(y + i, x, ACS_VLINE);
        mvaddch(y + i, x + w - 1, ACS_VLINE);
    }
    
    // Draw the title on the top border
    if (!title.empty()) {
        mvprintw(y, x + 2, "[ %s ]", title.c_str());
    }
}
