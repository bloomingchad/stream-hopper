#include <fcntl.h>
#include <ncurses.h>
#include <unistd.h>

#include <ctime>
#include <fstream> // Required for std::ifstream
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "CliHandler.h"
#include "FirstRunWizard.h" // Include the new wizard
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
    std::cout << "  (no command)         Launches the interactive radio player." << std::endl;
    std::cout << "                       If 'stations.jsonc' is not found, a setup wizard will run." << std::endl;
    std::cout << "  --from <file>        Launches the player with a specific station file." << std::endl;
    std::cout << "  --curate <genre>     Starts an interactive session to curate stations for a genre." << std::endl;
    std::cout << "  --list-tags          Lists popular, available genres from the Radio Browser API." << std::endl;
    std::cout << "  --help, -h           Displays this help message." << std::endl;
    std::cout << "\nEXAMPLE WORKFLOW:" << std::endl;
    std::cout << "  1. First Run:       ./build/stream-hopper (The setup wizard will run automatically)" << std::endl;
    std::cout << "  2. Discover genres: ./build/stream-hopper --list-tags" << std::endl;
    std::cout << "  3. Curate a list:   ./build/stream-hopper --curate techno" << std::endl;
    std::cout << "  4. Play your list:  ./build/stream-hopper --from techno.jsonc" << std::endl;
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
                std::string full_genre;
                // Combine all arguments after '--curate' into a single string
                for (int i = 2; i < argc; ++i) {
                    full_genre += argv[i];
                    if (i < argc - 1) {
                        full_genre += " ";
                    }
                }
                cli_handler.handle_curate_genre(full_genre);
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
    bool is_default_file = true;
    if (argc > 1 && std::string(argv[1]) == "--from") {
        if (argc > 2) {
            station_file_to_load = argv[2];
            is_default_file = false;
        } else {
            std::cerr << "Error: --from flag requires a filename." << std::endl;
            print_help();
            return 1;
        }
    }

    // --- First Run Check ---
    if (is_default_file) {
        std::ifstream f(station_file_to_load);
        if (!f.good()) {
            FirstRunWizard wizard;
            bool success = wizard.run();
            if (!success) {
                // The wizard's destructor will call endwin(), so we can print to std::cout
                std::cout << "Setup cancelled. Exiting." << std::endl;
                return 0;
            }
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
