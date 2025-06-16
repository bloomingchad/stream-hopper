// src/radio.cpp
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <ncurses.h>
#include "RadioPlayer.h"
#include "Utils.h"
#include "StationList.hpp" // <-- ADDED THIS INCLUDE

int main() {
    // The station_data vector is now loaded from StationList.hpp
    // This makes the list easy to edit without touching main application logic.

    try {
        RadioPlayer player(stream_hopper::station_data); // <-- Use the constant from the header
        player.run();
    } catch (const std::exception& e) {
        if (stdscr != NULL && !isendwin()) {
            endwin();
        }
        std::cerr << "Critical Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Radio player exited gracefully." << std::endl;
    return 0;
}
