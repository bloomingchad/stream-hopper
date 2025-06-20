#include "RadioPlayer.h"
#include "UIManager.h"
#include "StationManager.h"
#include "RadioStream.h"
#include "Utils.h"
#include <ncurses.h>
#include <iostream>
#include <algorithm>

namespace {
    constexpr int AUTO_HOP_TOTAL_TIME_SECONDS = 1125;
    constexpr int DISCOVERY_MODE_REFRESH_MS = 1000;
    constexpr int NORMAL_MODE_REFRESH_MS = 20;
    constexpr int COPY_MODE_TIMEOUT_SECONDS = 10;
    constexpr int FORGOTTEN_MUTE_MINUTES = 10;
    constexpr int COPY_MODE_REFRESH_MS = 100;
    constexpr int FOCUS_MODE_SECONDS = 90;
    constexpr size_t MAX_NAV_HISTORY = 10;
}

RadioPlayer::RadioPlayer(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data) {
    m_app_state = std::make_unique<AppState>();
    m_station_manager = std::make_unique<StationManager>(station_data, *m_app_state);
    m_ui = std::make_unique<UIManager>();

    m_input_handlers = {
        {KEY_UP,       [this]{ onUpArrow(); }},
        {KEY_DOWN,     [this]{ onDownArrow(); }},
        {KEY_ENTER,    [this]{ onEnter(); }},
        {'\n',         [this]{ onEnter(); }},
        {'\r',         [this]{ onEnter(); }},
        {'a',          [this]{ onToggleAutoHopMode(); }},
        {'A',          [this]{ onToggleAutoHopMode(); }},
        {'f',          [this]{ onToggleFavorite(); }},
        {'F',          [this]{ onToggleFavorite(); }},
        {'d',          [this]{ onToggleDucking(); }},
        {'D',          [this]{ onToggleDucking(); }},
        {'c',          [this]{ onCopyMode(); }},
        {'C',          [this]{ onCopyMode(); }},
        {'p',          [this]{ onToggleHopperMode(); }},
        {'P',          [this]{ onToggleHopperMode(); }},
        {'q',          [this]{ onQuit(); }},
        {'Q',          [this]{ onQuit(); }},
        {'\t',         [this]{ onSwitchPanel(); }},
        {KEY_RESIZE,   []{} }
    };
}

RadioPlayer::~RadioPlayer() {
    m_app_state->quit_flag = true;
    m_station_manager.reset();
    if (m_ui) {
        m_ui.reset();
    }
    
    if (m_app_state->was_quit_by_mute_timeout) {
        std::cout << "Hey, you forgot about me for " << FORGOTTEN_MUTE_MINUTES << " minutes! 😤" << std::endl;
    } else {
        auto end_time = std::chrono::steady_clock::now();
        auto duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_time - m_app_state->session_start_time).count();
        
        long duration_minutes = duration_seconds / 60;
        std::cout << "---\n"
                    << "Thank you for using Stream Hopper!\n"
                    << "🎛️ Session Switches: " << m_app_state->session_switches << "\n"
                    << "✨ New Songs Found: " << m_app_state->new_songs_found << "\n"
                    << "📋 Songs Copied: " << m_app_state->songs_copied << "\n"
                    << "🕐 Total Time: " << duration_minutes << " minutes\n"
                    << "---" << std::endl;
    }
}

void RadioPlayer::run() {
    m_app_state->needs_redraw = true;

    while (!m_app_state->quit_flag) {
        if (m_app_state->needs_redraw.exchange(false)) {
            auto station_data_snapshot = m_station_manager->getStationDisplayData();
            m_ui->draw(station_data_snapshot, *m_app_state,
                       getRemainingSecondsForCurrentStation(), getStationSwitchDuration(station_data_snapshot.size()));
        }

        int ch = m_ui->getInput();

        if (ch != ERR) {
            handleInput(ch);
        }

        updateState();
    }
}

void RadioPlayer::updateState() {
    if (m_app_state->copy_mode_active) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_app_state->copy_mode_start_time);
        if (elapsed.count() >= COPY_MODE_TIMEOUT_SECONDS) {
            onCopyMode();
        }
    }

    if (m_app_state->auto_hop_mode_active) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_app_state->auto_hop_start_time);
        
        auto station_count = m_station_manager->getStationDisplayData().size();
        if (elapsed.count() >= getStationSwitchDuration(station_count)) {
            onDownArrow();
            m_app_state->auto_hop_start_time = std::chrono::steady_clock::now();
        }
    }

    if (!m_app_state->auto_hop_mode_active && m_app_state->hopper_mode != HopperMode::FOCUS) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_app_state->last_switch_time);
        if (elapsed.count() >= FOCUS_MODE_SECONDS) {
            m_station_manager->post(Msg::SetHopperMode{HopperMode::FOCUS});
        }
    }
}

void RadioPlayer::handleInput(int ch) {
    if (m_app_state->copy_mode_active) {
        onCopyMode();
        return;
    }

    auto it = m_input_handlers.find(ch);
    if (it != m_input_handlers.end()) {
        it->second();
        m_app_state->needs_redraw = true;
    }
}

int RadioPlayer::getStationSwitchDuration(size_t station_count) const {
    if (station_count == 0) return 0;
    return AUTO_HOP_TOTAL_TIME_SECONDS / static_cast<int>(station_count);
}

int RadioPlayer::getRemainingSecondsForCurrentStation() const {
    if (!m_app_state->auto_hop_mode_active) return 0;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_app_state->auto_hop_start_time);
    
    auto station_count = m_station_manager->getStationDisplayData().size();
    return std::max(0, getStationSwitchDuration(station_count) - static_cast<int>(elapsed.count()));
}

void RadioPlayer::onUpArrow() {
    if (m_app_state->hopper_mode == HopperMode::FOCUS) {
        m_station_manager->post(Msg::SetHopperMode{HopperMode::BALANCED});
    }

    if (m_app_state->active_panel == ActivePanel::STATIONS) {
        auto station_count = m_station_manager->getStationDisplayData().size();
        if (station_count == 0) return;

        int old_idx = m_app_state->active_station_idx;
        int new_idx = (old_idx > 0) ? old_idx - 1 : static_cast<int>(station_count) - 1;
        m_station_manager->post(Msg::SwitchStation{old_idx, new_idx});
        m_app_state->active_station_idx = new_idx;
        m_app_state->nav_history.push_back({NavDirection::UP, std::chrono::steady_clock::now()});
        if (m_app_state->nav_history.size() > MAX_NAV_HISTORY) m_app_state->nav_history.pop_front();
        m_station_manager->post(Msg::UpdateActiveWindow{});
        m_app_state->history_scroll_offset = 0;
    } else {
        if (m_app_state->history_scroll_offset > 0) m_app_state->history_scroll_offset--;
    }
}

void RadioPlayer::onDownArrow() {
    if (m_app_state->hopper_mode == HopperMode::FOCUS) {
        m_station_manager->post(Msg::SetHopperMode{HopperMode::BALANCED});
    }

    if (m_app_state->active_panel == ActivePanel::STATIONS) {
        auto station_count = m_station_manager->getStationDisplayData().size();
        if (station_count == 0) return;

        int old_idx = m_app_state->active_station_idx;
        int new_idx = (old_idx < static_cast<int>(station_count) - 1) ? old_idx + 1 : 0;
        m_station_manager->post(Msg::SwitchStation{old_idx, new_idx});
        m_app_state->active_station_idx = new_idx;
        m_app_state->nav_history.push_back({NavDirection::DOWN, std::chrono::steady_clock::now()});
        if (m_app_state->nav_history.size() > MAX_NAV_HISTORY) m_app_state->nav_history.pop_front();
        m_station_manager->post(Msg::UpdateActiveWindow{});
        m_app_state->history_scroll_offset = 0;
    } else {
        auto stations = m_station_manager->getStationDisplayData();
        if (stations.empty()) return;
        const auto& name = stations[m_app_state->active_station_idx].name;
        size_t size = m_app_state->getStationHistorySize(name);
        if (size > 0 && m_app_state->history_scroll_offset < (int)size - 1) {
            m_app_state->history_scroll_offset++;
        }
    }
}

void RadioPlayer::onEnter() {
    m_station_manager->post(Msg::ToggleMute{m_app_state->active_station_idx});
}

void RadioPlayer::onToggleAutoHopMode() {
    m_app_state->auto_hop_mode_active = !m_app_state->auto_hop_mode_active;
    if (m_app_state->auto_hop_mode_active) {
        m_ui->setInputTimeout(DISCOVERY_MODE_REFRESH_MS);
        m_app_state->last_switch_time = std::chrono::steady_clock::now();
        m_app_state->auto_hop_start_time = std::chrono::steady_clock::now();
        
        auto stations = m_station_manager->getStationDisplayData();
        if(!stations.empty()) {
            const auto& station = stations[m_app_state->active_station_idx];
            if (station.playback_state != PlaybackState::Playing) onEnter();
            else if (station.current_volume < 50.0) m_station_manager->post(Msg::SwitchStation{m_app_state->active_station_idx, m_app_state->active_station_idx});
        }
    } else {
        m_ui->setInputTimeout(NORMAL_MODE_REFRESH_MS);
    }
}

void RadioPlayer::onToggleFavorite() {
    m_station_manager->post(Msg::ToggleFavorite{m_app_state->active_station_idx});
}

void RadioPlayer::onToggleDucking() {
    m_station_manager->post(Msg::ToggleDucking{m_app_state->active_station_idx});
}

void RadioPlayer::onCopyMode() {
    m_app_state->copy_mode_active = !m_app_state->copy_mode_active;
    if (m_app_state->copy_mode_active) {
        m_app_state->copy_mode_start_time = std::chrono::steady_clock::now();
        m_ui->setInputTimeout(COPY_MODE_REFRESH_MS);
    } else {
        m_ui->setInputTimeout(m_app_state->auto_hop_mode_active ? DISCOVERY_MODE_REFRESH_MS : NORMAL_MODE_REFRESH_MS);
    }
}

void RadioPlayer::onToggleHopperMode() {
    HopperMode next_mode = (m_app_state->hopper_mode == HopperMode::PERFORMANCE) ? HopperMode::BALANCED : HopperMode::PERFORMANCE;
    m_station_manager->post(Msg::SetHopperMode{next_mode});
}

void RadioPlayer::onQuit() {
    m_app_state->quit_flag = true;
}

void RadioPlayer::onSwitchPanel() {
    m_app_state->active_panel = (m_app_state->active_panel == ActivePanel::STATIONS) ? ActivePanel::HISTORY : ActivePanel::STATIONS;
}
