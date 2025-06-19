#include "UI/UIUtils.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <ncurses.h> // for colors

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

    std::stringstream time_ss;
    if (entry_time >= today_start) {
        time_ss << std::put_time(&tm, "%H:%M");
        return time_ss.str();
    }

    auto yesterday_start = today_start - std::chrono::hours(24);
    if (entry_time >= yesterday_start) {
        return "Yesterday";
    }

    time_ss << std::put_time(&tm, "%b %d");
    return time_ss.str();
}

void draw_box(int y, int x, int w, int h, const std::string& title, bool is_focused) {
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
