#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <fcntl.h> // For open
#include <unistd.h> // For dup2, close
#include <ctime>
#include <ncurses.h> // <-- FIX: Add missing ncurses header
#include "RadioPlayer.h"
#include "StationManager.h"
#include "StationList.hpp"

void suppress_stderr() {
    int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null == -1) {
        return;
    }
    dup2(dev_null, 2);
    close(dev_null);
}

int main() {
    suppress_stderr();

    try {
        StationManager manager(stream_hopper::station_data);
        RadioPlayer player(manager);
        player.run();
    } catch (const std::exception& e) {
        // FIX: The ncurses functions are only safe to call if ncurses has been initialized.
        // The UIManager destructor handles this now, so we just need to ensure it's called
        // by the natural stack unwinding of the 'try' block. We can remove these unsafe calls.

        FILE* logfile = fopen("stream_hopper_crash.log", "a");
        if (logfile) {
            time_t now = time(0);
            char dt[30];
            strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", localtime(&now));
            fprintf(logfile, "[%s] Critical Error: %s\n", dt, e.what());
            fclose(logfile);
        }
        std::cout << "\n\nA critical error occurred: " << e.what() << std::endl;
        std::cout << "The application must close. Details have been logged to stream_hopper_crash.log" << std::endl;
        return 1;
    }

    return 0;
}
