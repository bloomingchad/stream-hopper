// src/UIManager.cpp
#include "UIManager.h"
#include "RadioStream.h"
#include "AppState.h"

// UI Components
#include "UI/HeaderBar.h"
#include "UI/FooterBar.h"
#include "UI/StationsPanel.h"
#include "UI/NowPlayingPanel.h"
#include "UI/HistoryPanel.h"

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

UIManager::UIManager() {
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
    init_pair(5, COLOR_WHITE, COLOR_BLUE);    // High quality bitrate (background)
    init_pair(6, COLOR_WHITE, COLOR_GREEN);   // Medium quality bitrate (background)
    init_pair(7, COLOR_WHITE, COLOR_YELLOW);  // Low quality bitrate (background)
    init_pair(8, COLOR_BLACK, -1); // For A_DIM

    signal(SIGWINCH, UIManager::handle_resize);

    // Initialize UI components
    m_header_bar = std::make_unique<HeaderBar>();
    m_footer_bar = std::make_unique<FooterBar>();
    m_stations_panel = std::make_unique<StationsPanel>();
    m_now_playing_panel = std::make_unique<NowPlayingPanel>();
    m_history_panel = std::make_unique<HistoryPanel>();
}

UIManager::~UIManager() {
    if (stdscr != NULL && !isendwin()) {
        endwin();
    }
}

void UIManager::setInputTimeout(int milliseconds) {
    timeout(milliseconds);
}

void UIManager::draw(const std::vector<RadioStream>& stations, const AppState& app_state,
                     int remaining_seconds, int total_duration) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    if (width < COMPACT_MODE_WIDTH) {
        draw_compact_mode(width, height, stations, app_state, remaining_seconds, total_duration);
    } else {
        draw_full_mode(width, height, stations, app_state, remaining_seconds, total_duration);
    }
    
    refresh();
}

int UIManager::getInput() {
    if (s_resize_pending.exchange(false)) {
        // This is a reliable way to tell ncurses to check the new terminal size
        endwin();
        refresh();
        return KEY_RESIZE;
    }
    return getch();
}

void UIManager::draw_compact_mode(int width, int height, const std::vector<RadioStream>& stations, const AppState& app_state,
                                  int remaining_seconds, int total_duration) {
    // --- Layout Calculation ---
    double display_vol = 0.0;
    if (!stations.empty()) {
        const auto& active_station = stations[app_state.active_station_idx];
        if (active_station.isInitialized()) {
            display_vol = active_station.getPlaybackState() == PlaybackState::Muted ? 0.0 : active_station.getCurrentVolume();
        }
    }

    int now_playing_h = app_state.auto_hop_mode_active ? 6 : 5;
    int content_h = height - 2; // -2 for header/footer
    int remaining_h = content_h - now_playing_h;
    int stations_h = std::max(3, static_cast<int>(remaining_h * 0.6));
    int history_h = remaining_h - stations_h;

    if (history_h < 3) {
        stations_h = remaining_h;
        history_h = 0;
    }

    int now_playing_y = 1;
    int stations_y = now_playing_y + now_playing_h;
    int history_y = stations_y + stations_h;

    // --- Set Dimensions ---
    m_header_bar->setDimensions(0, 0, width, 1);
    m_footer_bar->setDimensions(height - 1, 0, width, 1);
    m_now_playing_panel->setDimensions(now_playing_y, 0, width, now_playing_h);
    m_stations_panel->setDimensions(stations_y, 0, width, stations_h);
    if(history_h > 0) m_history_panel->setDimensions(history_y, 0, width, history_h);

    // --- Delegate Drawing ---
    m_header_bar->draw(display_vol, app_state);
    m_footer_bar->draw(true, app_state);

    if (stations.empty()) return;
    const RadioStream& active_station = stations[app_state.active_station_idx];
    
    m_now_playing_panel->draw(active_station, app_state.auto_hop_mode_active, remaining_seconds, total_duration);
    m_stations_panel->draw(stations, app_state, app_state.active_panel == ActivePanel::STATIONS && !app_state.copy_mode_active);
    if (history_h > 0) {
        m_history_panel->draw(active_station, app_state, app_state.active_panel == ActivePanel::HISTORY && !app_state.copy_mode_active);
    }
}

void UIManager::draw_full_mode(int width, int height, const std::vector<RadioStream>& stations, const AppState& app_state,
                               int remaining_seconds, int total_duration) {
    // --- Layout Calculation ---
    double display_vol = 0.0;
    if (!stations.empty()) {
        const auto& active_station = stations[app_state.active_station_idx];
        if (active_station.isInitialized()) {
            display_vol = active_station.getPlaybackState() == PlaybackState::Muted ? 0.0 : active_station.getCurrentVolume();
        }
    }

    int content_h = height - 2; // -2 for header/footer
    int left_panel_w = std::max(35, width / 3);
    int right_panel_w = width - left_panel_w;
    int top_right_h = app_state.auto_hop_mode_active ? 7 : 6;
    int bottom_right_h = content_h - top_right_h;

    // --- Set Dimensions ---
    m_header_bar->setDimensions(0, 0, width, 1);
    m_footer_bar->setDimensions(height - 1, 0, width, 1);
    m_stations_panel->setDimensions(1, 0, left_panel_w, content_h);
    m_now_playing_panel->setDimensions(1, left_panel_w, right_panel_w, top_right_h);
    m_history_panel->setDimensions(1 + top_right_h, left_panel_w, right_panel_w, bottom_right_h);

    // --- Delegate Drawing ---
    m_header_bar->draw(display_vol, app_state);
    m_footer_bar->draw(false, app_state);

    if (stations.empty()) return;
    const RadioStream& current_station = stations[app_state.active_station_idx];
    
    m_stations_panel->draw(stations, app_state, app_state.active_panel == ActivePanel::STATIONS && !app_state.copy_mode_active);
    m_now_playing_panel->draw(current_station, app_state.auto_hop_mode_active, remaining_seconds, total_duration);
    m_history_panel->draw(current_station, app_state, app_state.active_panel == ActivePanel::HISTORY && !app_state.copy_mode_active);
}
