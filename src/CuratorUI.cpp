#include "CuratorUI.h"

#include <locale.h>
#include <ncurses.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <chrono>
#include <thread>
#include <cstdlib>

namespace {
    constexpr int COLOR_PAIR_DEFAULT = 1;
    constexpr int COLOR_PAIR_HEADER = 2;
    constexpr int COLOR_PAIR_ACCENT = 3;
    constexpr int COLOR_PAIR_DIM = 4;
    constexpr int COLOR_PAIR_KEEP = 5;
    constexpr int COLOR_PAIR_DISCARD = 6;
    constexpr int COLOR_PAIR_TAG = 7;

    // Background colors for quality pills
    constexpr int COLOR_PAIR_VIOLET_BG = 11;
    constexpr int COLOR_PAIR_BLUE_BG = 12;
    constexpr int COLOR_PAIR_GREEN_BG = 13;
    constexpr int COLOR_PAIR_CYAN_BG = 14;
    constexpr int COLOR_PAIR_ORANGE_BG = 15;
    constexpr int COLOR_PAIR_RED_BG = 16;

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

    // Initialize background colors for quality pills
    // Pair: ID, Foreground, Background
    init_pair(COLOR_PAIR_VIOLET_BG, COLOR_WHITE, COLOR_MAGENTA);
    init_pair(COLOR_PAIR_BLUE_BG, COLOR_WHITE, COLOR_BLUE);
    init_pair(COLOR_PAIR_GREEN_BG, COLOR_BLACK, COLOR_GREEN);
    init_pair(COLOR_PAIR_CYAN_BG, COLOR_BLACK, COLOR_CYAN);
    init_pair(COLOR_PAIR_ORANGE_BG, COLOR_BLACK, COLOR_YELLOW);
    init_pair(COLOR_PAIR_RED_BG, COLOR_WHITE, COLOR_RED);
}

void CuratorUI::draw_progress_bar(int y, int x, int width, int current, int total) {
    float ratio = (total > 0) ? static_cast<float>(current) / total : 0.0f;
    int filled_width = static_cast<int>(ratio * width);

    attron(COLOR_PAIR(COLOR_PAIR_ACCENT));
    mvprintw(y, x, "[");
    for (int i = 0; i < width; ++i) {
        if (i < filled_width) {
            addstr("█");
        } else {
            addstr("░");
        }
    }
    addstr("]");
    attroff(COLOR_PAIR(COLOR_PAIR_ACCENT));

    std::string progress_text = " " + std::to_string(current) + "/" + std::to_string(total);
    mvprintw(y, x + width + 3, "%s", progress_text.c_str());
}

void CuratorUI::draw_rating_stars(int votes) {
    if (votes <= 0) {
        attron(COLOR_PAIR(COLOR_PAIR_DIM));
        printw("☆☆☆☆☆");
        attroff(COLOR_PAIR(COLOR_PAIR_DIM));
        return;
    }
    
    int score = std::min(5, static_cast<int>(log10(votes + 1) / log10(100)) + 1);
    attron(COLOR_PAIR(COLOR_PAIR_ACCENT));
    for (int i = 0; i < score; ++i) {
        addstr("★");
    }
    for (int i = score; i < 5; ++i) {
        addstr("☆");
    }
    attroff(COLOR_PAIR(COLOR_PAIR_ACCENT));
    
    printw(" (%d)", votes);
}

// New helper function to draw the quality as a colored "pill"
void CuratorUI::draw_quality_pill(int y, int x, int bitrate) {
    mvprintw(y, x, "📶 Quality: ");

    std::string label;
    int color_pair;

    if (bitrate >= 288) {
        label = "VERY HIGH"; color_pair = COLOR_PAIR_VIOLET_BG;
    } else if (bitrate >= 176) {
        label = "HIGH"; color_pair = COLOR_PAIR_BLUE_BG;
    } else if (bitrate >= 144) {
        label = "GOOD"; color_pair = COLOR_PAIR_GREEN_BG;
    } else if (bitrate >= 104) {
        label = "NORMAL"; color_pair = COLOR_PAIR_CYAN_BG;
    } else if (bitrate >= 56) {
        label = "LOW"; color_pair = COLOR_PAIR_ORANGE_BG;
    } else {
        label = "VERY LOW"; color_pair = COLOR_PAIR_RED_BG;
    }

    attron(COLOR_PAIR(color_pair));
    printw(" %s ", label.c_str());
    attroff(COLOR_PAIR(color_pair));

    printw(" (%dkbps)", bitrate);
}


std::string CuratorUI::truncate_string(const std::string& str, int max_width) {
    if (static_cast<int>(str.length()) <= max_width) return str;
    return str.substr(0, std::max(0, max_width - 3)) + "...";
}

void CuratorUI::draw_tag_editor(int y, int x, const std::vector<std::string>& tags) {
    mvprintw(y, x, "🏷️ Tags: ");
    int current_x = x + 10;
    
    for (const auto& tag : tags) {
        int tag_width = tag.length() + 4;
        if (current_x + tag_width >= COLS - 2) break;
        
        attron(COLOR_PAIR(COLOR_PAIR_TAG));
        mvprintw(y, current_x, " %s ", tag.c_str());
        attroff(COLOR_PAIR(COLOR_PAIR_TAG));
        current_x += tag_width;
    }
    
    // Add edit indicator
    attron(COLOR_PAIR(COLOR_PAIR_ACCENT));
    mvprintw(y, current_x, " [E]");
    attroff(COLOR_PAIR(COLOR_PAIR_ACCENT));
}

void CuratorUI::draw(const std::string& genre,
                     int current_index,
                     int total_candidates,
                     int kept_count,
                     int discarded_count,
                     const CuratorStation& station,
                     const std::string& status,
                     bool is_playing) {
    clear();
    int width = COLS;
    
    // Header
    attron(COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
    mvprintw(1, 3, "🎵 STREAM HOPPER CURATOR 🎵");
    attroff(COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
    
    attron(COLOR_PAIR(COLOR_PAIR_DIM));
    mvprintw(2, 5, "GENRE: %s", genre.c_str());
    attroff(COLOR_PAIR(COLOR_PAIR_DIM));
    
    // Progress bar with stats
    int progress_width = std::min(40, width - 30);
    draw_progress_bar(4, 5, progress_width, current_index + 1, total_candidates);
    
    mvprintw(4, progress_width + 15, "KEPT: ");
    attron(COLOR_PAIR(COLOR_PAIR_KEEP) | A_BOLD);
    printw("%d", kept_count);
    attroff(COLOR_PAIR(COLOR_PAIR_KEEP) | A_BOLD);
    
    mvprintw(4, progress_width + 25, "DISCARDED: ");
    attron(COLOR_PAIR(COLOR_PAIR_DISCARD) | A_BOLD);
    printw("%d", discarded_count);
    attroff(COLOR_PAIR(COLOR_PAIR_DISCARD) | A_BOLD);
    
    // Station name
    int y_pos = 6;
    attron(A_BOLD);
    mvprintw(y_pos, std::max(3, (width - static_cast<int>(station.name.length())) / 2), 
             "%s", station.name.c_str());
    attroff(A_BOLD);
    
    // Now Playing
    attron(COLOR_PAIR(COLOR_PAIR_ACCENT));
    std::string status_label = is_playing ? "▶ PLAYING: " : "⏸ PAUSED: ";
    std::string full_status = status_label + status;
    std::string truncated_status = truncate_string(full_status, width - 10);
    mvprintw(y_pos + 2, std::max(3, (width - static_cast<int>(truncated_status.length())) / 2), 
             "%s", truncated_status.c_str());
    attroff(COLOR_PAIR(COLOR_PAIR_ACCENT));
    
    // Separator
    attron(COLOR_PAIR(COLOR_PAIR_DIM));
    for (int i = 0; i < width - 10; ++i) {
        mvaddstr(y_pos + 4, 5 + i, "─");
    }
    
    // Metadata
    y_pos += 6;
    mvprintw(y_pos, 5, "🌍 Country: %s", station.country_code.c_str());
    
    // Draw the quality pill and dynamically place the rating next to it.
    draw_quality_pill(y_pos, 25, station.bitrate);
    
    int cur_y, cur_x;
    getyx(stdscr, cur_y, cur_x); // Get current cursor position
    mvprintw(cur_y, cur_x + 3, "⭐ Rating: ");
    draw_rating_stars(station.votes);
    
    // Format information
    y_pos += 2;
    mvprintw(y_pos, 5, "🔊 Format: %s", station.format.c_str());
    
    // Tags with edit indicator
    y_pos += 2;
    draw_tag_editor(y_pos, 5, station.tags);
    
    // Footer with enhanced actions
    y_pos = LINES - 3;
    attron(COLOR_PAIR(COLOR_PAIR_DIM));
    mvprintw(y_pos, 5, "[K]eep   [D]iscard   [P]lay/Mute   [B]ack   [E]dit Tags   [Q]uit & Save");
    attroff(COLOR_PAIR(COLOR_PAIR_DIM));
    
    refresh();
}
