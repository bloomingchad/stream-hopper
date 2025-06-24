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

// A workaround for libmpv, which can sometimes print status or error
// messages to stderr even when configured not to. This function redirects
// the stderr file descriptor to /dev/null, silencing it completely for
// the TUI modes.
void suppress_stderr() {
    int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null == -1) {
        return;
    }
    dup2(dev_null, 2);
    close(dev_null);
}

void print_help() {
    std::cout << "stream-hopper: A terminal-based radio player and curator." << std::endl;
    std::cout << "\nUSAGE:" << std::endl;
    std::cout << "  ./build/stream-hopper [COMMAND]" << std::endl;
    std::cout << "\nCOMMANDS:" << std::endl;
    std::cout << "  (no command)         Launches the interactive radio player with 'stations.jsonc'." << std::endl;
    std::cout << "  --from <file>        Launches the player with a specific station file." << std::endl;
    std::cout << "  --curate <genre>     Starts an interactive session to curate stations for a genre." << std::endl;
    std::cout << "  --list-tags          Lists popular, available genres from the Radio Browser API." << std::endl;
    std::cout << "  --help, -h           Displays this help message." << std::endl;
    std::cout << "\nEXAMPLE WORKFLOW:" << std::endl;
    std::cout << "  1. Discover genres: ./build/stream-hopper --list-tags" << std::endl;
    std::cout << "  2. Curate a list:   ./build/stream-hopper --curate techno" << std::endl;
    std::cout << "  3. Play your list:  ./build/stream-hopper --from techno.jsonc" << std::endl;
}

int main(int argc, const char* argv[]) {
    // --- Command-line mode dispatcher ---
    if (argc > 1) {
        std::string arg = argv[1];
        CliHandler cli_handler;

        if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        }

        if (arg == "--list-tags") {
            cli_handler.handle_list_tags();
            return 0;
        }

        if (arg == "--curate") {
            if (argc > 2) {
                cli_handler.handle_curate_genre(argv[2]);
            } else {
                std::cerr << "Error: --curate flag requires a genre." << std::endl;
                print_help();
            }
            return 0;
        }

        // The '--from' flag is handled below with the main player logic.
        // If the arg is not '--from' and not a known command, it's an error.
        if (arg != "--from") {
            std::cerr << "Error: Unknown command '" << arg << "'." << std::endl;
            print_help();
            return 1;
        }
    }

    // --- Player Mode ---
    std::string station_file_to_load = "stations.jsonc";
    if (argc > 1 && std::string(argv[1]) == "--from") {
        if (argc > 2) {
            station_file_to_load = argv[2];
        } else {
            std::cerr << "Error: --from flag requires a filename." << std::endl;
            print_help();
            return 1;
        }
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
