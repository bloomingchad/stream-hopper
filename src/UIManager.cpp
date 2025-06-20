#include "UIManager.h"
#include "UI/StateSnapshot.h"
#include "UI/HeaderBar.h"
#include "UI/FooterBar.h"
#include "UI/StationsPanel.h"
#include "UI/NowPlayingPanel.h"
#include "UI/HistoryPanel.h"
#include "UI/Layout/CompactLayoutStrategy.h"
#include "UI/Layout/FullLayoutStrategy.h"
#include <ncurses.h>
#include <string>
#include <vector>
#include <csignal>
#include <locale.h>
#include <algorithm>

namespace { constexpr int COMPACT_MODE_WIDTH = 80; constexpr int DEFAULT_INPUT_TIMEOUT = 100; }
std::atomic<bool> UIManager::s_resize_pending = false;
void UIManager::resize_handler_trampoline(int) { s_resize_pending = true; }
UIManager::UIManager() : m_is_compact_mode(false) {
    setlocale(LC_ALL, ""); setlocale(LC_NUMERIC, "C"); initscr(); cbreak(); noecho();
    curs_set(0); keypad(stdscr, TRUE); timeout(DEFAULT_INPUT_TIMEOUT); start_color();
    use_default_colors(); init_pair(1, COLOR_YELLOW, -1); init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_CYAN, -1); init_pair(4, COLOR_MAGENTA, -1); init_pair(5, COLOR_WHITE, COLOR_BLUE);
    init_pair(6, COLOR_WHITE, COLOR_GREEN); init_pair(7, COLOR_WHITE, COLOR_YELLOW); init_pair(8, COLOR_BLACK, -1);
    signal(SIGWINCH, UIManager::resize_handler_trampoline);
    m_header_bar = std::make_unique<HeaderBar>(); m_footer_bar = std::make_unique<FooterBar>();
    m_stations_panel = std::make_unique<StationsPanel>(); m_now_playing_panel = std::make_unique<NowPlayingPanel>();
    m_history_panel = std::make_unique<HistoryPanel>();
    int width, height; getmaxyx(stdscr, width, height); (void)height; updateLayoutStrategy(width);
}
UIManager::~UIManager() { if (stdscr != NULL && !isendwin()) { endwin(); } }
void UIManager::setInputTimeout(int milliseconds) { timeout(milliseconds); }
void UIManager::handleResize() { endwin(); refresh(); }
void UIManager::updateLayoutStrategy(int width) {
    bool should_be_compact = (width < COMPACT_MODE_WIDTH);
    if (!m_layout_strategy || m_is_compact_mode != should_be_compact) {
        if (should_be_compact) m_layout_strategy = std::make_unique<CompactLayoutStrategy>();
        else m_layout_strategy = std::make_unique<FullLayoutStrategy>();
        m_is_compact_mode = should_be_compact;
    }
}

void UIManager::draw(const StateSnapshot& snapshot) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);
    updateLayoutStrategy(width);
    m_layout_strategy->calculateDimensions(width, height,
        *m_header_bar, *m_footer_bar, *m_stations_panel,
        *m_now_playing_panel, *m_history_panel, snapshot
    );
    m_header_bar->draw(snapshot.current_volume_for_header, snapshot.hopper_mode);
    m_footer_bar->draw(m_is_compact_mode, snapshot.is_copy_mode_active, snapshot.is_auto_hop_mode_active);

    if (snapshot.stations.empty()) {
        refresh();
        return;
    }
    const StationDisplayData& current_station = snapshot.stations[snapshot.active_station_idx];
    m_stations_panel->draw(snapshot.stations, snapshot.active_station_idx, snapshot.active_panel == ActivePanel::STATIONS && !snapshot.is_copy_mode_active);
    
    // FIX: The call is now simpler as NowPlayingPanel gets the full snapshot.
    m_now_playing_panel->draw(snapshot);
    
    m_history_panel->draw(current_station, snapshot.active_station_history, snapshot.history_scroll_offset, snapshot.active_panel == ActivePanel::HISTORY && !snapshot.is_copy_mode_active);
    refresh();
}

int UIManager::getInput() {
    if (s_resize_pending.exchange(false)) {
        handleResize();
        return KEY_RESIZE;
    }
    return getch();
}
