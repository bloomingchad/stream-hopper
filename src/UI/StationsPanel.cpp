#include "UI/StationsPanel.h"
#include "RadioStream.h"
#include "AppState.h"
#include "UI/UIUtils.h"
#include <ncurses.h>

StationsPanel::StationsPanel() : m_station_scroll_offset(0) {}

std::string StationsPanel::getStationStatusString(const RadioStream& station) const {
    if (!station.isInitialized()) {
        return "   "; // Default to blank for uninitialized/shutdown streams
    }
    if (station.isBuffering()) {
        return "🤔 ";
    }
    // Muted is a special state that overrides the volume check
    if (station.getPlaybackState() == PlaybackState::Muted) {
        return "🔇 ";
    }
    if (station.getCurrentVolume() > 0.1) {
        switch (station.getPlaybackState()) {
            case PlaybackState::Playing: return "▶️ ";
            case PlaybackState::Ducked:  return "🎧 ";
            default: break; // Muted case is handled above
        }
    }
    return "   "; // Fallback for volume 0 but not explicitly muted
}

void StationsPanel::drawStationLine(int y, const RadioStream& station, bool is_selected, int inner_w) {
    // 1. Set attributes based on state
    if (is_selected) {
        attron(A_REVERSE);
    } else if (!station.isInitialized()) {
        attron(A_DIM);
    }

    // 2. Build the display string
    std::string status_icon = getStationStatusString(station);
    std::string fav_icon = station.isFavorite() ? "⭐ " : "  ";
    std::string line = status_icon + fav_icon + station.getName();

    // 3. Draw the line
    mvwprintw(stdscr, y, m_x + 2, "%-*s", inner_w + 1, truncate_string(line, inner_w).c_str());

    // 4. Unset attributes
    if (is_selected) {
        attroff(A_REVERSE);
    } else if (!station.isInitialized()) {
        attroff(A_DIM);
    }
}

void StationsPanel::draw(const std::vector<RadioStream>& stations, const AppState& app_state, bool is_focused) {
    draw_box(m_y, m_x, m_w, m_h, "STATIONS", is_focused);
    int inner_w = m_w - 4;
    int active_station_idx = app_state.active_station_idx;

    // Adjust scroll offset to keep active station in view
    int visible_items = m_h > 2 ? m_h - 2 : 0;
    if (active_station_idx < m_station_scroll_offset) {
        m_station_scroll_offset = active_station_idx;
    }
    if (active_station_idx >= m_station_scroll_offset + visible_items) {
        m_station_scroll_offset = active_station_idx - visible_items + 1;
    }

    // The main loop is now much simpler
    for (int i = 0; i < visible_items; ++i) {
        int station_idx = m_station_scroll_offset + i;
        if (station_idx >= (int)stations.size()) break;

        bool is_selected = (station_idx == active_station_idx);
        drawStationLine(m_y + 1 + i, stations[station_idx], is_selected, inner_w);
    }
}
