#include "UIManager.h"
#include "AppState.h"
#include "UI/StationSnapshot.h"

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

namespace {
    constexpr int COMPACT_MODE_WIDTH = 80;
    constexpr int DEFAULT_INPUT_TIMEOUT = 100;
}

std::atomic<bool> UIManager::s_resize_pending = false;

void UIManager::handle_resize(int /* signum */) {
    s_resize_pending = true;
}

UIManager::UIManager() : m_is_compact_mode(false) {
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(DEFAULT_INPUT_TIMEOUT);
    start_color();
    use_default_colors();
    init_pair(1, COLOR_YELLOW, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_CYAN, -1);
    init_pair(4, COLOR_MAGENTA, -1);
    init_pair(5, COLOR_WHITE, COLOR_BLUE);
    init_pair(6, COLOR_WHITE, COLOR_GREEN);
    init_pair(7, COLOR_WHITE, COLOR_YELLOW);
    init_pair(8, COLOR_BLACK, -1);

    signal(SIGWINCH, UIManager::handle_resize);

    m_header_bar = std::make_unique<HeaderBar>();
    m_footer_bar = std::make_unique<FooterBar>();
    m_stations_panel = std::make_unique<StationsPanel>();
    m_now_playing_panel = std::make_unique<NowPlayingPanel>();
    m_history_panel = std::make_unique<HistoryPanel>();

    int width, height;
    getmaxyx(stdscr, width, height);
    updateLayoutStrategy(width);
}

UIManager::~UIManager() {
    if (stdscr != NULL && !isendwin()) {
        endwin();
    }
}

void UIManager::setInputTimeout(int milliseconds) {
    timeout(milliseconds);
}

void UIManager::updateLayoutStrategy(int width) {
    bool should_be_compact = (width < COMPACT_MODE_WIDTH);
    if (!m_layout_strategy || m_is_compact_mode != should_be_compact) {
        if (should_be_compact) {
            m_layout_strategy = std::make_unique<CompactLayoutStrategy>();
        } else {
            m_layout_strategy = std::make_unique<FullLayoutStrategy>();
        }
        m_is_compact_mode = should_be_compact;
    }
}

void UIManager::draw(const StationSnapshot& snapshot, const AppState& app_state,
                     int remaining_seconds, int total_duration) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    updateLayoutStrategy(width);

    m_layout_strategy->calculateDimensions(width, height,
        *m_header_bar, *m_footer_bar,
        *m_stations_panel, *m_now_playing_panel, *m_history_panel,
        app_state
    );
    
    double display_vol = 0.0;
    if (!snapshot.stations.empty()) {
        const auto& active_station = snapshot.stations[snapshot.active_station_idx];
        if (active_station.is_initialized) {
            display_vol = active_station.playback_state == PlaybackState::Muted ? 0.0 : active_station.current_volume;
        }
    }
    
    m_header_bar->draw(display_vol, app_state);
    m_footer_bar->draw(m_is_compact_mode, app_state);

    if (snapshot.stations.empty()) {
        refresh();
        return;
    }
    
    const StationDisplayData& current_station = snapshot.stations[snapshot.active_station_idx];
    
    m_stations_panel->draw(snapshot.stations, app_state, app_state.active_panel == ActivePanel::STATIONS && !app_state.copy_mode_active);
    m_now_playing_panel->draw(current_station, app_state.auto_hop_mode_active, remaining_seconds, total_duration);
    m_history_panel->draw(current_station, app_state, app_state.active_panel == ActivePanel::HISTORY && !app_state.copy_mode_active);
    
    refresh();
}

int UIManager::getInput() {
    if (s_resize_pending.exchange(false)) {
        endwin();
        refresh();
        return KEY_RESIZE;
    }
    return getch();
}
