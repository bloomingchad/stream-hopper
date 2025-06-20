#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <ctime>
#include <ncurses.h>
#include "RadioPlayer.h"
#include "StationManager.h"
#include "PersistenceManager.h" // <-- New include

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
        // Step 1: Load the station data from the file first.
        PersistenceManager persistence;
        StationData station_data = persistence.loadStations();

        // Step 2: Pass the loaded data to the StationManager.
        StationManager manager(station_data);
        RadioPlayer player(manager);
        
        // Step 3: Run the application.
        player.run();

    } catch (const std::exception& e) {
        // Graceful error handling for file I/O or parsing errors.
        if (stdscr != NULL && !isendwin()) {
            endwin();
        }
        
        std::cout << "\n\nA critical error occurred during startup:\n" << e.what() << std::endl;
        std::cout << "The application must close." << std::endl;
        
        // Log to file as a fallback.
        FILE* logfile = fopen("stream_hopper_crash.log", "a");
        if (logfile) {
            time_t now = time(0);
            char dt[30];
            strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", localtime(&now));
            fprintf(logfile, "[%s] Critical Error: %s\n", dt, e.what());
            fclose(logfile);
        }
        return 1;
    }

    return 0;
}
