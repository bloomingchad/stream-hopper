#include "RadioPlayer.h"
#include "UIManager.h"
#include "StationManager.h"
#include "UI/StateSnapshot.h"
#include "Utils.h"
#include <ncurses.h>
#include <iostream>
#include <thread>

namespace {
    // How often to send a "poke" message to the actor if no user input occurs.
    constexpr auto ACTOR_POLL_INTERVAL = std::chrono::milliseconds(100);
    // A short sleep to prevent the UI loop from using 100% CPU when idle.
    constexpr auto UI_IDLE_SLEEP = std::chrono::milliseconds(10);
}

RadioPlayer::RadioPlayer(StationManager& manager) 
    : m_station_manager(manager) 
{
    m_ui = std::make_unique<UIManager>();
    m_input_handlers = {
        {KEY_UP,       Msg::NavigateUp{}}, {KEY_DOWN,     Msg::NavigateDown{}},
        {KEY_ENTER,    Msg::ToggleMute{}}, {'\n',         Msg::ToggleMute{}},
        {'\r',         Msg::ToggleMute{}}, {'a',          Msg::ToggleAutoHop{}},
        {'A',          Msg::ToggleAutoHop{}}, {'f',          Msg::ToggleFavorite{}},
        {'F',          Msg::ToggleFavorite{}}, {'d',          Msg::ToggleDucking{}},
        {'D',          Msg::ToggleDucking{}}, {'c',          Msg::ToggleCopyMode{}},
        {'C',          Msg::ToggleCopyMode{}}, {'p',          Msg::ToggleHopperMode{}},
        {'P',          Msg::ToggleHopperMode{}}, {'q',          Msg::Quit{}},
        {'Q',          Msg::Quit{}}, {'\t',         Msg::SwitchPanel{}},
    };
}

RadioPlayer::~RadioPlayer() = default;

void RadioPlayer::run() {
    auto last_poll_time = std::chrono::steady_clock::now();

    while (!m_station_manager.getQuitFlag()) {
        // FIX: The correct event-driven loop. Atomically check and reset the flag.
        if (m_station_manager.getNeedsRedrawFlag().exchange(false)) {
            auto snapshot = m_station_manager.createSnapshot();
            m_ui->draw(snapshot);
            m_ui->setInputTimeout(snapshot.is_copy_mode_active ? -1 : 100);
        }

        int ch = m_ui->getInput();

        if (ch != ERR) {
            // A resize is not a message for the actor, it's for the UI manager.
            if (ch == KEY_RESIZE) {
                 m_station_manager.getNeedsRedrawFlag() = true; // Force a redraw after resize
                 continue;
            }
            
            // For all other keys, post the appropriate message.
            // The actor will set the redraw flag when it processes the message.
            auto snapshot = m_station_manager.createSnapshot();
            if (snapshot.is_copy_mode_active) {
                m_station_manager.post(Msg::ToggleCopyMode{});
            } else if (m_input_handlers.count(ch)) {
                m_station_manager.post(m_input_handlers.at(ch));
            }
        } else { // Timeout occurred, no user input.
            // Check if it's time for a periodic poll to drive actor timers.
            auto now = std::chrono::steady_clock::now();
            if (now - last_poll_time > ACTOR_POLL_INTERVAL) {
                m_station_manager.post(Msg::UpdateAndPoll{});
                last_poll_time = now;
            }
            // Sleep for a short duration to prevent this loop from busy-spinning.
            std::this_thread::sleep_for(UI_IDLE_SLEEP);
        }
    }
}
