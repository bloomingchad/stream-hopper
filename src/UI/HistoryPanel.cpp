#include "UI/HistoryPanel.h"
#include "RadioStream.h"
#include "AppState.h"
#include "UI/UIUtils.h"
#include "nlohmann/json.hpp"
#include <ncurses.h>
#include <iomanip>
#include <sstream>

void HistoryPanel::draw(const RadioStream& station, const AppState& app_state, bool is_focused) {
    if (m_h <= 0) return;
    draw_box(m_y, m_x, m_w, m_h, "📝 RECENT HISTORY", is_focused);

    const auto station_history = app_state.getStationHistory(station.getName());
    const int scroll_offset = app_state.history_scroll_offset;

    if (!station_history.empty()) {
        int inner_w = m_w - 5;
        int panel_height = m_h - 2;

        int display_count = 0;
        auto start_it = station_history.rbegin();
        if (scroll_offset < (int)station_history.size()) {
            std::advance(start_it, scroll_offset);
        }

        for (auto it = start_it; it != station_history.rend() && display_count < panel_height; ++it, ++display_count) {
            const auto& entry = *it;
            if (entry.is_array() && entry.size() == 2) {
                std::string full_ts = entry[0].get<std::string>();
                std::string title_str = entry[1].get<std::string>();
                
                std::string time_str = format_history_timestamp(full_ts);
                
                std::stringstream line_ss;
                line_ss << std::setw(9) << std::left << time_str << "│ " << title_str;
                
                mvwprintw(stdscr, m_y + 1 + display_count, m_x + 3, "%s", truncate_string(line_ss.str(), inner_w).c_str());
            }
        }
    }
}
