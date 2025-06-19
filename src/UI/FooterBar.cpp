#include "UI/FooterBar.h"
#include "AppState.h"
#include "UI/UIUtils.h"
#include <ncurses.h>
#include <string>

void FooterBar::draw(bool is_compact, const AppState& app_state) {
    std::string footer_text;
    if (app_state.copy_mode_active) {
        footer_text = " [COPY MODE] UI Paused. Press any key to resume... ";
    } else if (app_state.auto_hop_mode_active) {
        footer_text = " [A] Stop Auto-Hop   [C] Copy Mode   [Q] Quit ";
    } else if (is_compact) {
        footer_text = " [P] Mode [A] Auto [Nav] [Tab] Panel [F] Fav [D] Duck [C] Copy [Q] Quit ";
    } else {
        footer_text = " [P] Mode [A] Auto-Hop [↑↓] Nav [↵] Mute [D] Duck [⇥] Panel [F] Fav [C] Copy [Q] Quit ";
    }
    
    attron(A_REVERSE);
    mvprintw(m_y, m_x, "%*s", m_w, "");

    if (app_state.copy_mode_active) {
        attron(COLOR_PAIR(4));
        attron(A_BOLD);
    }
    
    if ((int)footer_text.length() < m_w) {
        mvprintw(m_y, (m_w - footer_text.length()) / 2, "%s", footer_text.c_str());
    } else {
        mvprintw(m_y, 1, "%s", truncate_string(footer_text, m_w - 2).c_str());
    }
    
    if (app_state.copy_mode_active) {
        attroff(A_BOLD);
        attroff(COLOR_PAIR(4));
    }
    attroff(A_REVERSE);
}
