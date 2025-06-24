#include <fcntl.h>
#include <ncurses.h>
#include <unistd.h>

#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "CliHandler.h"
#include "PersistenceManager.h"
#include "RadioPlayer.h"
#include "StationManager.h"

// This function is now only used by Player mode.
void suppress_stderr() {
    int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null == -1) {
        return;
    }
    dup2(dev_null, 2);
    close(dev_null);
}

int main(int argc, const char* argv[]) {
    // --- Command-line mode dispatcher ---
    if (argc > 1) {
        std::string arg = argv[1];
        CliHandler cli_handler;

        if (arg == "--list-tags") {
            cli_handler.handle_list_tags();
            return 0;
        }
        if (arg == "--curate") {
            if (argc > 2) {
                cli_handler.handle_curate_genre(argv[2]);
            } else {
                std::cerr << "Error: --curate flag requires a genre." << std::endl;
                std::cerr << "Example: ./build/stream-hopper --curate techno" << std::endl;
            }
            return 0;
        }
    }

    // --- Player Mode ---
    std::string station_file_to_load = "stations.jsonc";
    if (argc > 2 && std::string(argv[1]) == "--from") {
        station_file_to_load = argv[2];
    }

    suppress_stderr();

    try {
        PersistenceManager persistence;
        StationData station_data = persistence.loadStations(station_file_to_load);

        StationManager manager(station_data);
        RadioPlayer player(manager);

        player.run();

    } catch (const std::exception& e) {
        if (stdscr != NULL && !isendwin()) {
            endwin();
        }

        std::cout << "\n\nA critical error occurred during startup:\n" << e.what() << std::endl;
        std::cout << "The application must close." << std::endl;

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
