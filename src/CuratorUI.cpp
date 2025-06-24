#include "CuratorUI.h"

#include <locale.h>
#include <ncurses.h>

#include "UI/UIUtils.h"

CuratorUI::CuratorUI() {
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100); // Set a short timeout for getch()
    init_colors();
}

CuratorUI::~CuratorUI() {
    if (stdscr != NULL && !isendwin()) {
        endwin();
    }
}

void CuratorUI::init_colors() {
    start_color();
    use_default_colors();
    init_pair(1, COLOR_YELLOW, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_CYAN, -1);
}

void CuratorUI::draw(const std::string& genre,
                     int current_index,
                     int total_candidates,
                     int kept_count,
                     const std::string& station_name,
                     const std::string& status) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    // --- Header ---
    attron(A_REVERSE);
    mvprintw(0, 0, "%*s", width, "");
    mvprintw(0, 2, "STREAM HOPPER - CURATOR MODE");
    attroff(A_REVERSE);

    // --- Main Content ---
    mvprintw(height / 2 - 4, 4, "Curating stations for genre:");
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(height / 2 - 3, 4, "%s", genre.c_str());
    attroff(COLOR_PAIR(3) | A_BOLD);

    mvprintw(height / 2 - 1, 4, "Progress: %d / %d", current_index + 1, total_candidates);
    mvprintw(height / 2, 4, "Kept: %d", kept_count);

    mvprintw(height / 2 + 2, 4, "Now Reviewing:");
    attron(COLOR_PAIR(1));
    mvwprintw(stdscr, height / 2 + 3, 4, "-> %s", truncate_string(station_name, width - 10).c_str());
    attroff(COLOR_PAIR(1));

    mvprintw(height / 2 + 5, 4, "Status: %s", truncate_string(status, width - 15).c_str());

    // --- Footer ---
    std::string footer_text = "[K]eep | [D]iscard/Skip | [P]lay/Mute | [Q]uit & Save";
    attron(A_REVERSE);
    mvprintw(height - 1, 0, "%*s", width, "");
    mvprintw(height - 1, (width - footer_text.length()) / 2, "%s", footer_text.c_str());
    attroff(A_REVERSE);

    refresh();
}
