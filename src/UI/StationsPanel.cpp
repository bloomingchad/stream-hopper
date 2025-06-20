#include "UI/StationsPanel.h"
#include "UI/StationDisplayData.h"
#include "AppState.h"
#include "UI/UIUtils.h"
#include <ncurses.h>

StationsPanel::StationsPanel() : m_station_scroll_offset(0) {}

std::string StationsPanel::getStationStatusString(const StationDisplayData& station) const {
    if (!station.is_initialized) {
        return "   ";
    }
    if (station.is_buffering) {
        return "🤔 ";
    }
    if (station.playback_state == PlaybackState::Muted) {
        return "🔇 ";
    }
    if (station.current_volume > 0.1) {
        switch (station.playback_state) {
            case PlaybackState::Playing: return "▶️ ";
            case PlaybackState::Ducked:  return "🎧 ";
            default: break;
        }
    }
    return "   ";
}

void StationsPanel::drawStationLine(int y, const StationDisplayData& station, bool is_selected, int inner_w) {
    if (is_selected) {
        attron(A_REVERSE);
    } else if (!station.is_initialized) {
        attron(A_DIM);
    }

    std::string status_icon = getStationStatusString(station);
    std::string fav_icon = station.is_favorite ? "⭐ " : "  ";
    std::string line = status_icon + fav_icon + station.name;

    mvwprintw(stdscr, y, m_x + 2, "%-*s", inner_w + 1, truncate_string(line, inner_w).c_str());

    if (is_selected) {
        attroff(A_REVERSE);
    } else if (!station.is_initialized) {
        attroff(A_DIM);
    }
}

void StationsPanel::draw(const std::vector<StationDisplayData>& stations, const AppState& app_state, bool is_focused) {
    draw_box(m_y, m_x, m_w, m_h, "STATIONS", is_focused);
    int inner_w = m_w - 4;
    int active_station_idx = app_state.active_station_idx;

    int visible_items = m_h > 2 ? m_h - 2 : 0;
    if (active_station_idx < m_station_scroll_offset) {
        m_station_scroll_offset = active_station_idx;
    }
    if (active_station_idx >= m_station_scroll_offset + visible_items) {
        m_station_scroll_offset = active_station_idx - visible_items + 1;
    }

    for (int i = 0; i < visible_items; ++i) {
        int station_idx = m_station_scroll_offset + i;
        if (station_idx >= (int)stations.size()) break;

        bool is_selected = (station_idx == active_station_idx);
        drawStationLine(m_y + 1 + i, stations[station_idx], is_selected, inner_w);
    }
}
