// src/radio.cpp
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <ncurses.h>
#include <fcntl.h> // For open
#include <unistd.h> // For dup2, close
#include "RadioPlayer.h"
#include "StationList.hpp"

void suppress_stderr() {
    // Open /dev/null for writing
    int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null == -1) {
        return; // Failed, but we can continue without suppression
    }
    // Redirect stderr (file descriptor 2) to /dev/null
    dup2(dev_null, 2);
    close(dev_null);
}

int main() {
    // Suppress all stderr messages from libraries like libav
    suppress_stderr();

    try {
        RadioPlayer player(stream_hopper::station_data);
        player.run();
    } catch (const std::exception& e) {
        // This block will now catch errors from check_mpv_error
        if (stdscr != NULL && !isendwin()) {
            endwin();
        }
        // Since stderr is suppressed, we'll log to a file for critical errors
        FILE* logfile = fopen("stream_hopper_crash.log", "a");
        if (logfile) {
            time_t now = time(0);
            char dt[30];
            strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", localtime(&now));
            fprintf(logfile, "[%s] Critical Error: %s\n", dt, e.what());
            fclose(logfile);
        }
        // Also print to stdout for immediate user feedback
        std::cout << "A critical error occurred: " << e.what() << std::endl;
        std::cout << "Details have been logged to stream_hopper_crash.log" << std::endl;
        return 1;
    }

    // The final summary will now print to stdout, which is unaffected
    return 0;
}
