// CuratorUI.cpp
#include "CuratorUI.h"

#include <locale.h>
#include <ncurses.h>
#include <cmath>
#include <algorithm>

namespace {
    constexpr int COLOR_PAIR_DEFAULT = 1;
    constexpr int COLOR_PAIR_HEADER = 2;
    constexpr int COLOR_PAIR_ACCENT = 3;
    constexpr int COLOR_PAIR_DIM = 4;
    constexpr int COLOR_PAIR_KEEP = 5;
    constexpr int COLOR_PAIR_DISCARD = 6;
    constexpr int COLOR_PAIR_TAG = 7;
} // namespace

CuratorUI::CuratorUI() {
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100);
    init_colors();
}

CuratorUI::~CuratorUI() {
    if (stdscr != nullptr && !isendwin()) {
        endwin();
    }
}

void CuratorUI::init_colors() {
    start_color();
    use_default_colors();
    init_pair(COLOR_PAIR_DEFAULT, COLOR_WHITE, -1);
    init_pair(COLOR_PAIR_HEADER, COLOR_MAGENTA, -1);
    init_pair(COLOR_PAIR_ACCENT, COLOR_CYAN, -1);
    init_pair(COLOR_PAIR_DIM, 8, -1);
    init_pair(COLOR_PAIR_KEEP, COLOR_GREEN, -1);
    init_pair(COLOR_PAIR_DISCARD, COLOR_RED, -1);
    init_pair(COLOR_PAIR_TAG, COLOR_BLACK, COLOR_WHITE);
}

void CuratorUI::draw_progress_bar(int y, int x, int width, int current, int total) {
    float ratio = (total > 0) ? static_cast<float>(current) / total : 0.0f;
    int filled_width = static_cast<int>(ratio * width);

    attron(COLOR_PAIR(COLOR_PAIR_ACCENT));
    for (int i = 0; i < width; ++i) {
        if (i < filled_width) {
            mvaddstr(y, x + i, "█");
        } else {
            attron(COLOR_PAIR(COLOR_PAIR_DIM));
            mvaddstr(y, x + i, "─");
            attroff(COLOR_PAIR(COLOR_PAIR_DIM));
            attron(COLOR_PAIR(COLOR_PAIR_ACCENT));
        }
    }
    attroff(COLOR_PAIR(COLOR_PAIR_ACCENT));

    std::string progress_text = std::to_string(current) + " / " + std::to_string(total);
    mvprintw(y, x + width + 2, "%s", progress_text.c_str());
}

std::string CuratorUI::get_reliability_stars(int votes) {
    if (votes <= 0) return "☆☆☆☆☆";
    int score = std::min(5, static_cast<int>(log10(votes) / log10(50)) + 1);
    return std::string(score, '★') + std::string(5 - score, '☆');
}

std::string CuratorUI::truncate_string(const std::string& str, int max_width) {
    if ((int)str.length() <= max_width) return str;
    return str.substr(0, std::max(0, max_width - 3)) + "...";
}

void CuratorUI::draw(const std::string& genre,
                     int current_index,
                     int total_candidates,
                     int kept_count,
                     const CuratorStation& station,
                     const std::string& status) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    // Header
    attron(COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
    mvprintw(1, 3, "🎵 STREAM HOPPER 🎵");
    attroff(COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
    attron(COLOR_PAIR(COLOR_PAIR_DIM));
    mvprintw(2, 5, "CURATOR MODE: %s", genre.c_str());
    attroff(COLOR_PAIR(COLOR_PAIR_DIM));

    // Progress
    draw_progress_bar(4, 3, std::max(10, width - 30), current_index + 1, total_candidates);
    attron(COLOR_PAIR(COLOR_PAIR_DIM));
    mvprintw(4, width - 14, "Kept: %d", kept_count);
    attroff(COLOR_PAIR(COLOR_PAIR_DIM));

    // Station name
    int y_pos = 7;
    attron(COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);
    mvprintw(y_pos, std::max(0, (width - (int)station.name.length()) / 2), "%s", station.name.c_str());
    attroff(COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);

    // Now Playing
    attron(COLOR_PAIR(COLOR_PAIR_DEFAULT));
    std::string truncated_status = truncate_string(status, width - 10);
    mvprintw(y_pos + 2, std::max(0, (width - (int)truncated_status.length()) / 2 - 2), "▶  %s", truncated_status.c_str());

    // Separator
    attron(COLOR_PAIR(COLOR_PAIR_DIM));
    for (int i = 0; i < width - 10; ++i) {
        mvaddstr(y_pos + 4, 5 + i, "─");
    }

    // Metadata
    y_pos += 6;
    mvprintw(y_pos, 5, "🌍 Country: %s", station.country_code.c_str());
    mvprintw(y_pos, 25, "📶 Bitrate: %d kbps", station.bitrate);
    std::string stars = get_reliability_stars(station.votes);
    mvprintw(y_pos, 50, "⭐ Votes: %s (%d)", stars.c_str(), station.votes);

    // Tags
    int current_x = 13;
    mvprintw(y_pos + 2, 5, "🏷️ Tags: ");
    for (const auto& tag : station.tags) {
        int tag_width = tag.length() + 4;
        if (current_x + tag_width >= width - 2) break;
        attron(COLOR_PAIR(COLOR_PAIR_TAG));
        mvprintw(y_pos + 2, current_x, " %s ", tag.c_str());
        attroff(COLOR_PAIR(COLOR_PAIR_TAG));
        current_x += tag_width;
    }

    // Footer
    attron(COLOR_PAIR(COLOR_PAIR_DIM));
    mvprintw(height - 2, 5, "[K]eep   [D]iscard   [P]lay/Mute   [Q]uit & Save");
    attroff(COLOR_PAIR(COLOR_PAIR_DIM));

    refresh();
}
