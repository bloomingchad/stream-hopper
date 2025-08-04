#include "RadioPlayer.h"

#include <ncurses.h>

#include <cctype> // for tolower
#include <iostream>
#include <thread>

#include "StationManager.h"
#include "UI/StateSnapshot.h"
#include "UIManager.h"
#include "Utils.h"

namespace {
    constexpr auto ACTOR_POLL_INTERVAL = std::chrono::milliseconds(100);
    constexpr auto UI_IDLE_SLEEP = std::chrono::milliseconds(10);
}

RadioPlayer::RadioPlayer(StationManager& manager) : m_station_manager(manager) {
    m_ui = std::make_unique<UIManager>();
    m_input_handlers = {
        {KEY_UP, Msg::NavigateUp{}},
        {KEY_DOWN, Msg::NavigateDown{}},
        {KEY_LEFT, Msg::AdjustVolumeOffsetDown{}},
        {KEY_RIGHT, Msg::AdjustVolumeOffsetUp{}},
        {KEY_ENTER, Msg::ToggleMute{}},
        {' ', Msg::ToggleMute{}},
        {'\n', Msg::ToggleMute{}},
        {'\r', Msg::ToggleMute{}},
        {'a', Msg::ToggleAutoHop{}},
        {'f', Msg::ToggleFavorite{}},
        {'d', Msg::ToggleDucking{}},
        {'c', Msg::ToggleCopyMode{}},
        {'p', Msg::ToggleHopperMode{}},
        {'r', Msg::EnterRandomMode{}},
        {'q', Msg::Quit{}},
        {'\t', Msg::SwitchPanel{}},
        {'+', Msg::CycleUrl{}},
    };
}

RadioPlayer::~RadioPlayer() = default;

void RadioPlayer::run() {
    auto last_poll_time = std::chrono::steady_clock::now();

    while (!m_station_manager.getQuitFlag()) {
        if (m_station_manager.getNeedsRedrawFlag().exchange(false)) {
            auto snapshot = m_station_manager.createSnapshot();
            m_ui->draw(snapshot);
            m_ui->setInputTimeout(snapshot.is_copy_mode_active ? -1 : 100);
        }

        int ch = m_ui->getInput();

        if (ch != ERR) {
            if (ch == KEY_RESIZE) {
                m_station_manager.getNeedsRedrawFlag() = true;
                continue;
            }

            auto snapshot = m_station_manager.createSnapshot();
            if (snapshot.is_copy_mode_active) {
                // Pass the character directly if it's a letter.
                if (isalpha(ch)) {
                    m_station_manager.post(Msg::SearchOnline{(char) tolower(ch)});
                }
                // Always exit the mode after a key press
                m_station_manager.post(Msg::ToggleCopyMode{});

            } else {
                // Handle case-insensitivity for normal mode keys
                int lower_ch = tolower(ch);
                if (m_input_handlers.count(lower_ch)) {
                    m_station_manager.post(m_input_handlers.at(lower_ch));
                } else if (m_input_handlers.count(ch)) { // For non-alpha keys like KEY_UP
                    m_station_manager.post(m_input_handlers.at(ch));
                }
            }
        } else {
            auto now = std::chrono::steady_clock::now();
            if (now - last_poll_time > ACTOR_POLL_INTERVAL) {
                m_station_manager.post(Msg::UpdateAndPoll{});
                last_poll_time = now;
            }
            std::this_thread::sleep_for(UI_IDLE_SLEEP);
        }
    }
}
