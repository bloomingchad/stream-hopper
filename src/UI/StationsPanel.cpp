#include "UI/StationsPanel.h"
#include "RadioStream.h"
#include "AppState.h"
#include "UI/UIUtils.h"
#include <ncurses.h>

StationsPanel::StationsPanel() : m_station_scroll_offset(0) {}

void StationsPanel::draw(const std::vector<RadioStream>& stations, const AppState& app_state, bool is_focused) {
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

        const auto& station = stations[station_idx];
        bool is_selected = (station_idx == active_station_idx);
        
        if (is_selected) {
            attron(A_REVERSE);
        }
        
        if (!station.isInitialized() && !is_selected) {
            attron(A_DIM);
        }

        std::string status_icon = "   "; // Default to blank for uninitialized streams
        if (station.isInitialized()) {
            if (station.isBuffering()) {
                status_icon = "🤔 "; 
            } else if (station.getCurrentVolume() > 0.1) {
                switch(station.getPlaybackState()) {
                    case PlaybackState::Playing: status_icon = "▶️ "; break;
                    case PlaybackState::Muted:   status_icon = "🔇 "; break;
                    case PlaybackState::Ducked:  status_icon = "🎧 "; break;
                }
            } else if (station.getPlaybackState() == PlaybackState::Muted) {
                 status_icon = "🔇 ";
            }
        }
        
        std::string fav_icon = station.isFavorite() ? "⭐ " : "  ";
        std::string line = status_icon + fav_icon + station.getName();
        mvwprintw(stdscr, m_y + 1 + i, m_x + 2, "%-*s", inner_w + 1, truncate_string(line, inner_w).c_str());

        if (!station.isInitialized() && !is_selected) {
            attroff(A_DIM);
        }
        if (is_selected) {
            attroff(A_REVERSE);
        }
    }
}
