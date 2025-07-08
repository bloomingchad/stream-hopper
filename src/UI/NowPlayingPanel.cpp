#include "UI/NowPlayingPanel.h"

#include <ncurses.h>

#include <iomanip>
#include <sstream>
#include <vector>

#include "Core/VolumeNormalizer.h" // Include for MAX_OFFSET
#include "UI/UIUtils.h"

namespace {
    const std::vector<char> SPINNER_CHARS = {'|', '/', '-', '\\'};
    int spinner_idx = 0;
}

void NowPlayingPanel::draw(const StateSnapshot& snapshot) {
    if (snapshot.stations.empty())
        return;
    const auto& station = snapshot.stations[snapshot.active_station_idx];

    std::string box_title = snapshot.is_auto_hop_mode_active ? "🤖 AUTO-HOP MODE" : "▶️  NOW PLAYING";
    draw_box(m_y, m_x, m_w, m_h, box_title, false);

    int inner_w = m_w - 4;

    // This logic is now cleaner and fixes the "Unnecessary Copy" warning from infer.
    std::string title_to_show;
    if (!station.is_initialized) {
        title_to_show = "...";
    } else if (station.is_buffering) {
        title_to_show = "Buffering...";
    } else {
        // This covers both normal and cycling states, which both want current_title.
        // Future improvements could show pending_title here during a cycle.
        title_to_show = station.current_title;
    }

    attron(A_BOLD);
    mvwprintw(stdscr, m_y + 2, m_x + 3, "%s", truncate_string(title_to_show, inner_w - 2).c_str());
    attroff(A_BOLD);

    int bitrate = station.is_initialized ? station.bitrate : 0;
    std::string bitrate_str;
    if (bitrate > 0) {
        bitrate_str = " " + std::to_string(bitrate) + "k ";
    }

    if (station.cycling_state == CyclingState::IDLE) {
        size_t max_name_width = inner_w - bitrate_str.length() - 2;
        mvwprintw(stdscr, m_y + 3, m_x + 3, "%s", truncate_string(station.name, max_name_width).c_str());
    } else {
        drawCycleStatus(station, inner_w);
    }

    if (!bitrate_str.empty()) {
        int color_pair_num = 0;
        if (bitrate > 200)
            color_pair_num = 5;
        else if (bitrate >= 96)
            color_pair_num = 6;
        else
            color_pair_num = 7;

        attron(COLOR_PAIR(color_pair_num));
        mvwprintw(stdscr, m_y + 2, m_x + m_w - bitrate_str.length() - 3, "%s", bitrate_str.c_str());
        attroff(COLOR_PAIR(color_pair_num));
    }

    if (snapshot.is_auto_hop_mode_active) {
        drawAutoHopView(inner_w, snapshot.auto_hop_remaining_seconds, snapshot.auto_hop_total_duration);
    } else if (snapshot.is_volume_offset_mode_active) {
        drawVolumeOffsetBar(station, inner_w);
    } else {
        drawNormalView(station, inner_w);
    }
}

void NowPlayingPanel::drawVolumeOffsetBar(const StationDisplayData& station, int inner_w) {
    int bar_width = inner_w - 14; // Room for "🎚️ NORM [...] +10.0"
    if (bar_width <= 0)
        return;

    mvwprintw(stdscr, m_y + 1, m_x + 3, "🎚️ NORM [");
    int bar_start_x = m_x + 12;

    int center_point = bar_width / 2;
    double offset = station.volume_offset;
    // Use the public constant for the range calculation
    int fill_width = static_cast<int>((offset / VolumeNormalizer::MAX_OFFSET) * center_point);

    attron(COLOR_PAIR(9)); // Yellow
    for (int i = 0; i < bar_width; ++i) {
        if (fill_width > 0 && i >= center_point && i < center_point + fill_width) {
            mvwaddch(stdscr, m_y + 1, bar_start_x + i, ACS_BLOCK);
        } else if (fill_width < 0 && i < center_point && i >= center_point + fill_width) {
            mvwaddch(stdscr, m_y + 1, bar_start_x + i, ACS_BLOCK);
        } else if (i == center_point) {
            mvwaddch(stdscr, m_y + 1, bar_start_x + i, ACS_VLINE);
        } else {
            mvwaddch(stdscr, m_y + 1, bar_start_x + i, ACS_CKBOARD);
        }
    }
    attroff(COLOR_PAIR(9));

    mvwprintw(stdscr, m_y + 1, bar_start_x + bar_width, "]");

    std::stringstream ss;
    ss << std::fixed << std::showpos << std::setprecision(1) << offset;
    mvwprintw(stdscr, m_y + 1, bar_start_x + bar_width + 2, "%s", ss.str().c_str());
}

void NowPlayingPanel::drawCycleStatus(const StationDisplayData& station, int inner_w) {
    std::string status_text;
    switch (station.cycling_state) {
    case CyclingState::CYCLING: {
        spinner_idx = (spinner_idx + 1) % SPINNER_CHARS.size();
        char spinner = SPINNER_CHARS[spinner_idx];
        std::stringstream ss;
        std::string pending_str = (station.pending_bitrate > 0) ? std::to_string(station.pending_bitrate) + "k" : "...";
        ss << station.name << " [ " << station.bitrate << "k → " << pending_str << " " << spinner << " ]";
        status_text = ss.str();
        break;
    }
    case CyclingState::SUCCEEDED:
        status_text = station.name + " [ ✅ ]";
        break;
    case CyclingState::FAILED:
        status_text = station.name + " [ ❌ Failed ]";
        break;
    case CyclingState::IDLE:
        status_text = station.name;
        break;
    }
    mvwprintw(stdscr, m_y + 3, m_x + 3, "%s", truncate_string(status_text, inner_w).c_str());
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
        for (int i = 0; i < filled_width; ++i)
            mvwaddch(stdscr, m_y + m_h - 2, m_x + 3 + i, ACS_BLOCK);
        attroff(COLOR_PAIR(2));
        for (int i = filled_width; i < bar_width; ++i)
            mvwaddch(stdscr, m_y + m_h - 2, m_x + 3 + i, '.');
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
        for (int i = 0; i < filled_width; ++i)
            mvwaddch(stdscr, m_y + 1, m_x + 6 + i, ACS_BLOCK);
        attroff(COLOR_PAIR(2));
        for (int i = filled_width; i < bar_width; ++i)
            mvwaddch(stdscr, m_y + 1, m_x + 6 + i, ACS_CKBOARD);
        mvwprintw(stdscr, m_y + 1, m_x + 6 + bar_width, "]");
        mvwprintw(stdscr, m_y + 1, m_x + 8 + bar_width, "%.0f%%", is_muted ? 0.0 : station.current_volume);
    }
}
