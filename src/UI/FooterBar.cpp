#include "UI/FooterBar.h"

#include <ncurses.h>

#include <string>

#include "UI/UIUtils.h"

void FooterBar::draw(bool is_compact,
                     bool is_copy_mode_active,
                     bool is_auto_hop_mode_active,
                     bool can_cycle_url,
                     const std::string& temp_msg) {
    std::string footer_text;
    std::string cycle_text = can_cycle_url ? "[+] Cycle " : "";
    bool is_error_msg = false;

    if (!temp_msg.empty()) {
        footer_text = " " + temp_msg + " ";
        is_error_msg = true;
    } else if (is_copy_mode_active) {
        // This is now dynamically generated in the final version, but a static placeholder is fine for now
        // A more advanced version would get the provider list from the snapshot.
        footer_text = " [SEARCH] (Y)T Music (S)potify (A)pple (C)SoundCloud (D)eezer (B)andcamp (W)eb ";
    } else if (is_auto_hop_mode_active) {
        footer_text = " [A] Stop Auto-Hop   [C] Search Online   [Q] Quit ";
    } else if (is_compact) {
        footer_text = " [P] Mode [A] Auto [Nav] " + cycle_text + "[Tab] Panel [F] Fav [D] Duck [C] Search [Q] Quit ";
    } else {
        footer_text = " [P] Mode [A] Auto-Hop [↑↓] Nav [↵] Mute " + cycle_text +
                      "[D] Duck [⇥] Panel [F] Fav [C] Search [Q] Quit ";
    }

    attron(A_REVERSE);
    mvprintw(m_y, m_x, "%*s", m_w, "");

    if (is_copy_mode_active || is_error_msg) {
        attron(COLOR_PAIR(4));
        attron(A_BOLD);
    }

    if ((int) footer_text.length() < m_w) {
        mvprintw(m_y, (m_w - footer_text.length()) / 2, "%s", footer_text.c_str());
    } else {
        mvprintw(m_y, 1, "%s", truncate_string(footer_text, m_w - 2).c_str());
    }

    if (is_copy_mode_active || is_error_msg) {
        attroff(A_BOLD);
        attroff(COLOR_PAIR(4));
    }
    attroff(A_REVERSE);
}
