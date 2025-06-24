#include "CuratorApp.h"

#include <ncurses.h>

#include "CuratorUI.h"

CuratorApp::CuratorApp(const std::string& genre, StationData candidates)
    : m_genre(genre), m_candidates(std::move(candidates)), m_current_index(0), m_quit_flag(false) {
    m_ui = std::make_unique<CuratorUI>();
}

CuratorApp::~CuratorApp() {
    // In a future step, this destructor will save the curated list.
}

void CuratorApp::run() {
    // In future steps, this will be the main interactive loop.
    // For now, it just draws the static UI once and waits for 'q'.
    while (!m_quit_flag) {
        m_ui->draw(m_genre, m_current_index, m_candidates.size(), m_kept_stations.size(),
                   m_candidates[m_current_index].first);

        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            m_quit_flag = true;
        }
    }
}
