#include "UI/FooterBar.h"
#include "UI/UIUtils.h"
#include <ncurses.h>
#include <string>

void FooterBar::draw(bool is_compact, bool is_copy_mode_active, bool is_auto_hop_mode_active) {
    std::string footer_text;
    if (is_copy_mode_active) {
        footer_text = " [COPY MODE] UI Paused. Press any key to resume... ";
    } else if (is_auto_hop_mode_active) {
        footer_text = " [A] Stop Auto-Hop   [C] Copy Mode   [Q] Quit ";
    } else if (is_compact) {
        // Adding [+] Cycle to compact mode
        footer_text = " [P] Mode [A] Auto [Nav] [+] Cycle [Tab] Panel [F] Fav [D] Duck [C] Copy [Q] Quit ";
    } else {
        // Adding [+] Cycle to full mode
        footer_text = " [P] Mode [A] Auto-Hop [↑↓] Nav [↵] Mute [+] Cycle [D] Duck [⇥] Panel [F] Fav [C] Copy [Q] Quit ";
    }
    
    attron(A_REVERSE);
    mvprintw(m_y, m_x, "%*s", m_w, "");

    if (is_copy_mode_active) {
        attron(COLOR_PAIR(4));
        attron(A_BOLD);
    }
    
    if ((int)footer_text.length() < m_w) {
        mvprintw(m_y, (m_w - footer_text.length()) / 2, "%s", footer_text.c_str());
    } else {
        mvprintw(m_y, 1, "%s", truncate_string(footer_text, m_w - 2).c_str());
    }
    
    if (is_copy_mode_active) {
        attroff(A_BOLD);
        attroff(COLOR_PAIR(4));
    }
    attroff(A_REVERSE);
}
