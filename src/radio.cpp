#include <fcntl.h>
#include <ncurses.h>
#include <unistd.h>

#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "PersistenceManager.h"
#include "RadioPlayer.h"
#include "StationManager.h"
#include "Utils.h"

void suppress_stderr() {
    int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null == -1) {
        return;
    }
    dup2(dev_null, 2);
    close(dev_null);
}

void handle_list_tags() {
    std::cout << "Fetching available genres from Radio Browser API..." << std::endl;
    try {
        // We call the script from the build directory.
        std::string raw_json_str = exec_process("./build/api_helper.sh --list-tags");

        if (raw_json_str.empty()) {
            std::cerr << "Error: Failed to fetch data. The helper script returned no output." << std::endl;
            std::cerr << "Please check your internet connection and ensure 'curl' is installed." << std::endl;
            return;
        }

        // In this step, we just print the raw output.
        // Parsing and cleaning will happen in the next step.
        std::cout << "\n--- RAW API RESPONSE ---" << std::endl;
        std::cout << raw_json_str << std::endl;
        std::cout << "------------------------\n" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\nAn error occurred while listing tags: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // --- Command-line mode dispatcher ---
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--list-tags") {
            handle_list_tags();
            return 0; // Exit after handling the utility command.
        }
    }

    // --- Default Player Mode ---
    suppress_stderr();

    try {
        PersistenceManager persistence;
        StationData station_data = persistence.loadStations();

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
