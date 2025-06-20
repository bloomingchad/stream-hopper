#include "UI/NowPlayingPanel.h"
#include "UI/StationSnapshot.h"
#include "AppState.h"
#include "UI/UIUtils.h"
#include <ncurses.h>

void NowPlayingPanel::draw(const StationDisplayData& station, bool is_auto_hop_mode,
                           int remaining_seconds, int total_duration) {
    std::string box_title = is_auto_hop_mode ? "🤖 AUTO-HOP MODE" : "▶️  NOW PLAYING";
    draw_box(m_y, m_x, m_w, m_h, box_title, false);

    int inner_w = m_w - 4;
    
    std::string title = station.is_initialized ? (station.is_buffering ? "Buffering..." : station.current_title) : "...";
    attron(A_BOLD);
    mvwprintw(stdscr, m_y + 2, m_x + 3, "%s", truncate_string(title, inner_w - 2).c_str());
    attroff(A_BOLD);
    
    int bitrate = station.is_initialized ? station.bitrate : 0;
    std::string bitrate_str;
    if (bitrate > 0) {
        bitrate_str = " " + std::to_string(bitrate) + "k ";
    }

    size_t max_name_width = inner_w - bitrate_str.length() - 2;
    mvwprintw(stdscr, m_y + 3, m_x + 3, "%s", truncate_string(station.name, max_name_width).c_str());

    if (!bitrate_str.empty()) {
        int color_pair_num = 0;
        if (bitrate > 200)      { color_pair_num = 5; }
        else if (bitrate >= 96) { color_pair_num = 6; }
        else                    { color_pair_num = 7; }
        
        attron(COLOR_PAIR(color_pair_num));
        mvwprintw(stdscr, m_y + 3, m_x + m_w - bitrate_str.length() - 3, "%s", bitrate_str.c_str());
        attroff(COLOR_PAIR(color_pair_num));
    }

    if (is_auto_hop_mode) {
        drawAutoHopView(inner_w, remaining_seconds, total_duration);
    } else {
        drawNormalView(station, inner_w);
    }
}

void NowPlayingPanel::drawAutoHopView(int inner_w, int remaining_seconds, int total_duration) {
    int bar_width = inner_w - 2;
    if (bar_width > 0) {
        double elapsed_percent = 0.0;
        if (total_duration > 0) {
            elapsed_percent = static_cast<double>(total_duration - remaining_seconds) / total_duration;
        }
        int filled_width = static_cast<int>(elapsed_percent * bar_width);

        mvwprintw(stdscr, m_y + m_h - 2, m_x + 2, "[");
        attron(COLOR_PAIR(2));
        for(int i = 0; i < filled_width; ++i) mvwaddch(stdscr, m_y + m_h - 2, m_x + 3 + i, ACS_BLOCK);
        attroff(COLOR_PAIR(2));
        for(int i = filled_width; i < bar_width; ++i) mvwaddch(stdscr, m_y + m_h - 2, m_x + 3 + i, '.');
        mvwprintw(stdscr, m_y + m_h - 2, m_x + 3 + bar_width, "]");
        
        std::string time_text = "Next in " + std::to_string(remaining_seconds) + "s";
        mvwprintw(stdscr, m_y + 1, m_x + m_w - time_text.length() - 2, "%s", time_text.c_str());
    }
}

void NowPlayingPanel::drawNormalView(const StationDisplayData& station, int inner_w) {
    int bar_width = inner_w - 12; 
    if (bar_width > 0) {
        bool is_muted = !station.is_initialized || station.playback_state == PlaybackState::Muted;
        double vol_percent = (is_muted ? 0.0 : station.current_volume) / 100.0;
        int filled_width = static_cast<int>(vol_percent * bar_width);
        
        mvwprintw(stdscr, m_y + 1, m_x + 3, "🔊 [");
        attron(COLOR_PAIR(2));
        for(int i = 0; i < filled_width; ++i) mvwaddch(stdscr, m_y + 1, m_x + 6 + i, ACS_BLOCK);
        attroff(COLOR_PAIR(2));
        for(int i = filled_width; i < bar_width; ++i) mvwaddch(stdscr, m_y + 1, m_x + 6 + i, ACS_CKBOARD);
        mvwprintw(stdscr, m_y + 1, m_x + 6 + bar_width, "]");
        mvwprintw(stdscr, m_y + 1, m_x + 8 + bar_width, "%.0f%%", is_muted ? 0.0 : station.current_volume);
    }
}
