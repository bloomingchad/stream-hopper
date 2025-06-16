#include "RadioPlayer.h"
#include "UIManager.h"
#include "StationManager.h" // Include new class
#include "RadioStream.h" // Still needed for some constants/checks
#include "Utils.h" // For forgotten mute check

#include <ncurses.h> // For ERR
#include <iostream>  // For exit messages
#include <algorithm> // For std::any_of

// Constants that define application behavior
#define SMALL_MODE_TOTAL_TIME_SECONDS 1125
#define DISCOVERY_MODE_REFRESH_MS 1000
#define NORMAL_MODE_REFRESH_MS 100
#define COPY_MODE_TIMEOUT_SECONDS 10
#define FORGOTTEN_MUTE_SECONDS 600
#define COPY_MODE_REFRESH_MS 100

RadioPlayer::RadioPlayer(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data) {
    m_app_state = std::make_unique<AppState>();
    m_station_manager = std::make_unique<StationManager>(station_data, *m_app_state);
    m_ui = std::make_unique<UIManager>();
}

RadioPlayer::~RadioPlayer() {
    m_app_state->quit_flag = true;
    m_station_manager->stopEventLoop();

    // --- Final Summary Logic ---
    // Ensure ncurses is shut down before we print to cout.
    if (m_ui) {
        m_ui.reset();
    }
    
    // Calculate session duration
    auto end_time = std::chrono::steady_clock::now();
    auto duration_seconds = std::chrono::duration_cast<std::chrono::seconds>(end_time - m_app_state->session_start_time).count();
    long duration_minutes = duration_seconds / 60;

    // Check for forgotten mute on exit
    bool forgot_mute = false;
    if (!m_station_manager->getStations().empty()) {
        const auto& stations = m_station_manager->getStations();
        const RadioStream& active_station = stations[m_app_state->active_station_idx];
        auto mute_start_time = active_station.getMuteStartTime();

        if (active_station.isMuted() && mute_start_time.has_value()) {
            auto mute_duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - mute_start_time.value());
            if (mute_duration.count() >= FORGOTTEN_MUTE_SECONDS) {
                forgot_mute = true;
                long minutes = mute_duration.count() / 60;
                std::string unit = (minutes == 1) ? " minute" : " minutes";
                std::cout << "hey you forgot about me for " << minutes << unit << " 😤" << std::endl;
            }
        }
    }

    if (!forgot_mute) {
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
    m_station_manager->runEventLoop();

    while (!m_app_state->quit_flag) {
        if (m_app_state->copy_mode_active) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_app_state->copy_mode_start_time);
            if (elapsed.count() >= COPY_MODE_TIMEOUT_SECONDS) {
                m_app_state->copy_mode_active = false;
                m_app_state->needs_redraw = true;
                m_ui->setInputTimeout(m_app_state->small_mode_active ? DISCOVERY_MODE_REFRESH_MS : NORMAL_MODE_REFRESH_MS);
            }
        } else {
            updateState();
        }

        if (m_app_state->needs_redraw.exchange(false)) {
            if (!m_app_state->copy_mode_active) {
                m_ui->draw(m_station_manager->getStations(), m_app_state->active_station_idx, m_app_state->getHistory(),
                           m_app_state->active_panel, m_app_state->history_scroll_offset, m_app_state->small_mode_active,
                           getRemainingSecondsForCurrentStation(), getStationSwitchDuration(), false);
            }
        }

        int ch = m_ui->getInput();
        if (ch != ERR) {
            handleInput(ch);
        }
    }
}

void RadioPlayer::updateState() {
    const auto& stations = m_station_manager->getStations();
    
    // Discovery mode station switching logic
    if (m_app_state->small_mode_active) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_app_state->small_mode_start_time);
        
        if (elapsed.count() >= getStationSwitchDuration()) {
            int old_idx = m_app_state->active_station_idx;
            int new_idx = (old_idx + 1) % static_cast<int>(stations.size());
            m_station_manager->switchStation(old_idx, new_idx);
            m_app_state->active_station_idx = new_idx;
            m_app_state->small_mode_start_time = std::chrono::steady_clock::now();
            m_app_state->needs_redraw = true;
        } else {
             m_app_state->needs_redraw = true; // Force redraw for timer
        }
    }
    
    // Redraw if any station is fading
    if (std::any_of(stations.cbegin(), stations.cend(), [](const RadioStream& s) { return s.isFading(); })) {
        m_app_state->needs_redraw = true;
    }

    // Forgotten mute check during runtime
    if (!m_app_state->small_mode_active && !stations.empty()) {
        const RadioStream& active_station = stations[m_app_state->active_station_idx];
        if (active_station.isMuted()) {
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
}

void RadioPlayer::handleInput(int ch) {
    if (m_app_state->copy_mode_active) {
        m_app_state->copy_mode_active = false;
        m_app_state->needs_redraw = true;
        m_ui->setInputTimeout(m_app_state->small_mode_active ? DISCOVERY_MODE_REFRESH_MS : NORMAL_MODE_REFRESH_MS);
        return;
    }

    const auto& stations = m_station_manager->getStations();
    int old_idx = m_app_state->active_station_idx;
    int new_idx = old_idx;

    switch (ch) {
        case KEY_UP:
            if (m_app_state->active_panel == ActivePanel::STATIONS) {
                new_idx = (old_idx > 0) ? old_idx - 1 : static_cast<int>(stations.size()) - 1;
                m_station_manager->switchStation(old_idx, new_idx);
                m_app_state->active_station_idx = new_idx;
                m_app_state->history_scroll_offset = 0;
            } else { // HISTORY
                if (m_app_state->history_scroll_offset > 0) m_app_state->history_scroll_offset--;
            }
            break;
        case KEY_DOWN:
            if (m_app_state->active_panel == ActivePanel::STATIONS) {
                new_idx = (old_idx < static_cast<int>(stations.size()) - 1) ? old_idx + 1 : 0;
                m_station_manager->switchStation(old_idx, new_idx);
                m_app_state->active_station_idx = new_idx;
                m_app_state->history_scroll_offset = 0;
            } else { // HISTORY
                const auto& history = m_app_state->getHistory();
                const auto& current_station_name = stations[old_idx].getName();
                if (history.contains(current_station_name)) {
                    if (m_app_state->history_scroll_offset < (int)history[current_station_name].size() - 1) {
                         m_app_state->history_scroll_offset++;
                    }
                }
            }
            break;
        case '\t':
            m_app_state->active_panel = (m_app_state->active_panel == ActivePanel::STATIONS) ? ActivePanel::HISTORY : ActivePanel::STATIONS;
            break;
        case '\n': case '\r': case KEY_ENTER:
            m_station_manager->toggleMuteStation(old_idx);
            break;
        case 's': case 'S':
            toggleSmallMode();
            break;
        case 'f': case 'F':
            m_station_manager->toggleFavorite(old_idx);
            break;
        case 'd': case 'D':
            m_station_manager->toggleAudioDucking(old_idx);
            break;
        case 'c': case 'C':
            m_app_state->copy_mode_active = true;
            m_app_state->copy_mode_start_time = std::chrono::steady_clock::now();
            m_ui->setInputTimeout(COPY_MODE_REFRESH_MS);
            m_ui->draw(stations, old_idx, m_app_state->getHistory(), m_app_state->active_panel, m_app_state->history_scroll_offset,
                       m_app_state->small_mode_active, getRemainingSecondsForCurrentStation(), getStationSwitchDuration(), true);
            m_app_state->songs_copied++; // <-- INCREMENT COPY COUNTER
            break;
        case 'q': case 'Q':
            m_app_state->quit_flag = true;
            break;
    }
    
    if (ch != 'c' && ch != 'C') {
       m_app_state->needs_redraw = true;
    }
}

int RadioPlayer::getStationSwitchDuration() {
    const auto& stations = m_station_manager->getStations();
    if (stations.empty()) return 0;
    return SMALL_MODE_TOTAL_TIME_SECONDS / static_cast<int>(stations.size());
}

int RadioPlayer::getRemainingSecondsForCurrentStation() {
    if (!m_app_state->small_mode_active) return 0;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_app_state->small_mode_start_time);
    return std::max(0, getStationSwitchDuration() - static_cast<int>(elapsed.count()));
}

void RadioPlayer::toggleSmallMode() {
    m_app_state->small_mode_active = !m_app_state->small_mode_active;
    if (m_app_state->small_mode_active) {
        m_ui->setInputTimeout(DISCOVERY_MODE_REFRESH_MS);
        m_app_state->small_mode_start_time = std::chrono::steady_clock::now();
        const auto& stations = m_station_manager->getStations();
        if(!stations.empty()) {
            const RadioStream& current_station = stations[m_app_state->active_station_idx];
            // Un-mute or restore volume when entering discovery mode
            if (current_station.isMuted() || current_station.isDucked()) {
                m_station_manager->toggleMuteStation(m_app_state->active_station_idx);
            } else if (current_station.getCurrentVolume() < 50.0) {
                 m_station_manager->switchStation(m_app_state->active_station_idx, m_app_state->active_station_idx); // "Switch" to self to restore volume
            }
        }
    } else {
        m_ui->setInputTimeout(NORMAL_MODE_REFRESH_MS);
    }
}
