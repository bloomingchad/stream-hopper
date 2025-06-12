// src/Utils.cpp
#include "Utils.h"
#include <iostream>

void check_mpv_error(int status, const std::string& context) {
    if (status < 0) {
        // Ensure ncurses is properly shut down before printing to stderr
        if (stdscr != NULL && !isendwin()) {
            endwin();
        }
        std::cerr << "MPV Error (" << context << "): " << mpv_error_string(status) << std::endl;
        exit(1); // Terminate the program on critical MPV error
    }
}
