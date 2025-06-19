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
    constexpr int FORGOTTEN_MUTE_SECONDS = 600;
    constexpr int COPY_MODE_REFRESH_MS = 100;
    constexpr int FOCUS_MODE_SECONDS = 90;
    constexpr size_t MAX_NAV_HISTORY = 10;
}

RadioPlayer::RadioPlayer(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data) {
    m_app_state = std::make_unique<AppState>();
    m_station_manager = std::make_unique<StationManager>(station_data, *m_app_state);
    m_ui = std::make_unique<UIManager>();
}

RadioPlayer::~RadioPlayer() {
    m_app_state->quit_flag = true;
    
    // --- FIX: Explicitly control the shutdown order ---
    // 1. Shut down the StationManager first. Its destructor will block until
    //    all of its threads (actor and fade threads) are completely finished.
    //    This guarantees no thread will try to access AppState after this point.
    m_station_manager.reset();

    // 2. Shut down the UI.
    if (m_ui) {
        m_ui.reset();
    }
    
    // 3. Now that all threads are stopped and the UI is closed, it's safe to
    //    access AppState to print the final summary.
    auto end_time = std::chrono::steady_clock::now();
    auto duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_time - m_app_state->session_start_time).count();

    // The logic for 'forgot_mute' was removed, so the variable is no longer needed.
    
    long duration_minutes = duration_seconds / 60;
    std::cout << "---\n"
                << "Thank you for using Stream Hopper!\n"
                << "🎛️ Session Switches: " << m_app_state->session_switches << "\n"
                << "✨ New Songs Found: " << m_app_state->new_songs_found << "\n"
                << "📋 Songs Copied: " << m_app_state->songs_copied << "\n"
                << "🕐 Total Time: " << duration_minutes << " minutes\n"
                << "---" << std::endl;
}

void RadioPlayer::run() {
    m_app_state->needs_redraw = true;

    while (!m_app_state->quit_flag) {
        // 1. First, check if a redraw is needed from any source (user input, timers, background threads).
        // This ensures the UI is always up-to-date before we wait for new input.
        if (m_app_state->needs_redraw.exchange(false)) {
            m_ui->draw(m_station_manager->getStations(), *m_app_state,
                       getRemainingSecondsForCurrentStation(), getStationSwitchDuration());
        }

        // 2. Now, get input. This is the only part of the loop that sleeps.
        int ch = m_ui->getInput();

        // 3. Handle any user input that we just received.
        if (ch != ERR) {
            handleInput(ch);
        }

        // 4. Handle any time-based state updates.
        updateState();
    }
}

void RadioPlayer::updateState() {
    const auto& stations = m_station_manager->getStations();
    
    // Handle copy mode timeout
    if (m_app_state->copy_mode_active) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_app_state->copy_mode_start_time);
        if (elapsed.count() >= COPY_MODE_TIMEOUT_SECONDS) {
            m_app_state->copy_mode_active = false;
            m_app_state->needs_redraw = true;
            m_ui->setInputTimeout(m_app_state->auto_hop_mode_active ? DISCOVERY_MODE_REFRESH_MS : NORMAL_MODE_REFRESH_MS);
        }
    }

    // Handle auto-hop mode timer
    if (m_app_state->auto_hop_mode_active) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_app_state->auto_hop_start_time);
        
        if (elapsed.count() >= getStationSwitchDuration()) {
            int old_idx = m_app_state->active_station_idx;
            int new_idx = (old_idx + 1) % static_cast<int>(stations.size());
            
            m_station_manager->post(Msg::SwitchStation{old_idx, new_idx});
            m_app_state->active_station_idx = new_idx;
            m_station_manager->post(Msg::UpdateActiveWindow{});

            m_app_state->auto_hop_start_time = std::chrono::steady_clock::now();
            m_app_state->needs_redraw = true; // Need to redraw the timer
        }
    }
    
    // Check for forgotten mute
    if (!m_app_state->auto_hop_mode_active && !stations.empty()) {
        const RadioStream& active_station = stations[m_app_state->active_station_idx];
        if (active_station.getPlaybackState() == PlaybackState::Muted) {
            auto mute_start = active_station.getMuteStartTime();
            if (mute_start.has_value()) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - mute_start.value());
                if (elapsed.count() >= FORGOTTEN_MUTE_SECONDS) {
                    m_app_state->quit_flag = true;
                }
            }
        }
    }

    // Check for automatic focus mode
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
        m_app_state->copy_mode_active = false;
        m_ui->setInputTimeout(m_app_state->auto_hop_mode_active ? DISCOVERY_MODE_REFRESH_MS : NORMAL_MODE_REFRESH_MS);
        m_app_state->needs_redraw = true;
        return;
    }

    switch (ch) {
        case KEY_UP:        onUpArrow();            break;
        case KEY_DOWN:      onDownArrow();          break;
        case '\n': case '\r': case KEY_ENTER: onEnter(); break;
        case 'a': case 'A': onToggleAutoHopMode();  break;
        case 'f': case 'F': onToggleFavorite();     break;
        case 'd': case 'D': onToggleDucking();      break;
        case 'c': case 'C': onCopyMode();           break;
        case 'p': case 'P': onToggleHopperMode();   break;
        case 'q': case 'Q': onQuit();               break;
        case '\t':          onSwitchPanel();        break;
        case KEY_RESIZE:    /* Handled by redraw flag */ break;
        default:            return;
    }
    
    // Every valid action should trigger a redraw
    m_app_state->needs_redraw = true;
}

int RadioPlayer::getStationSwitchDuration() {
    const auto& stations = m_station_manager->getStations();
    if (stations.empty()) return 0;
    return AUTO_HOP_TOTAL_TIME_SECONDS / static_cast<int>(stations.size());
}

int RadioPlayer::getRemainingSecondsForCurrentStation() {
    if (!m_app_state->auto_hop_mode_active) return 0;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_app_state->auto_hop_start_time);
    return std::max(0, getStationSwitchDuration() - static_cast<int>(elapsed.count()));
}

void RadioPlayer::onUpArrow() {
    if (m_app_state->hopper_mode == HopperMode::FOCUS) {
        m_station_manager->post(Msg::SetHopperMode{HopperMode::BALANCED});
    }

    const auto& stations = m_station_manager->getStations();
    if (m_app_state->active_panel == ActivePanel::STATIONS) {
        int old_idx = m_app_state->active_station_idx;
        int new_idx = (old_idx > 0) ? old_idx - 1 : static_cast<int>(stations.size()) - 1;
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

    const auto& stations = m_station_manager->getStations();
    if (m_app_state->active_panel == ActivePanel::STATIONS) {
        int old_idx = m_app_state->active_station_idx;
        int new_idx = (old_idx < static_cast<int>(stations.size()) - 1) ? old_idx + 1 : 0;
        m_station_manager->post(Msg::SwitchStation{old_idx, new_idx});
        m_app_state->active_station_idx = new_idx;
        
        m_app_state->nav_history.push_back({NavDirection::DOWN, std::chrono::steady_clock::now()});
        if (m_app_state->nav_history.size() > MAX_NAV_HISTORY) m_app_state->nav_history.pop_front();

        m_station_manager->post(Msg::UpdateActiveWindow{});
        m_app_state->history_scroll_offset = 0;
    } else {
        const auto& current_station_name = stations[m_app_state->active_station_idx].getName();
        size_t history_size = m_app_state->getStationHistorySize(current_station_name);
        if (history_size > 0 && m_app_state->history_scroll_offset < (int)history_size - 1) {
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
        const auto& stations = m_station_manager->getStations();
        if(!stations.empty()) {
            const RadioStream& current_station = stations[m_app_state->active_station_idx];
            PlaybackState current_state = current_station.getPlaybackState();
            if (current_state == PlaybackState::Muted || current_state == PlaybackState::Ducked) {
                m_station_manager->post(Msg::ToggleMute{m_app_state->active_station_idx});
            } else if (current_station.getCurrentVolume() < 50.0) {
                 m_station_manager->post(Msg::SwitchStation{m_app_state->active_station_idx, m_app_state->active_station_idx});
            }
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
    m_app_state->copy_mode_active = true;
    m_app_state->copy_mode_start_time = std::chrono::steady_clock::now();
    m_ui->setInputTimeout(COPY_MODE_REFRESH_MS);
}

void RadioPlayer::onToggleHopperMode() {
    HopperMode current_mode = m_app_state->hopper_mode;
    if (current_mode == HopperMode::BALANCED || current_mode == HopperMode::FOCUS) {
        m_station_manager->post(Msg::SetHopperMode{HopperMode::PERFORMANCE});
    } else if (current_mode == HopperMode::PERFORMANCE) {
        m_station_manager->post(Msg::SetHopperMode{HopperMode::BALANCED});
    }
}

void RadioPlayer::onQuit() {
    m_app_state->quit_flag = true;
}

void RadioPlayer::onSwitchPanel() {
    m_app_state->active_panel = (m_app_state->active_panel == ActivePanel::STATIONS) ? ActivePanel::HISTORY : ActivePanel::STATIONS;
}
