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
#include "FirstRunWizard.h"
#include "PersistenceManager.h"
#include "RadioPlayer.h"
#include "StationManager.h"

// --- Utility Functions ---

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

void log_critical_error(const std::exception& e) {
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
}

// --- Core Logic Functions ---

// Handles CLI commands that cause the program to exit immediately.
// Returns true if a command was handled, false otherwise.
bool handle_cli_commands(int argc, const char* argv[]) {
    if (argc <= 1) {
        return false; // No command, proceed to player mode
    }

    std::string arg = argv[1];
    CliHandler cli_handler;

    if (arg == "--help" || arg == "-h") {
        print_help();
        return true;
    }

    if (arg == "--list-tags") {
        cli_handler.handle_list_tags();
        return true;
    }

    if (arg == "--curate") {
        if (argc > 2) {
            std::string full_genre;
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
        return true; // Even if --curate had an error, it's a CLI command that should exit.
    }
    return false; // Not an immediate-exit command
}

// Determines the station file to load.
// Returns the filename. If an unknown command or misused --from is detected,
// it prints help and returns an empty string to signal main() to exit with error.
std::string determine_station_file(int argc, const char* argv[]) {
    std::string station_file = "stations.jsonc"; // Default

    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--from") {
            if (argc > 2) {
                station_file = argv[2];
            } else {
                std::cerr << "Error: --from flag requires a filename." << std::endl;
                print_help();
                return ""; // Indicate error
            }
        } else if (arg1 != "--help" && arg1 != "-h" && arg1 != "--list-tags" && arg1 != "--curate") {
            // This case handles an unknown first argument that isn't '--from'
            // and wasn't caught by handle_cli_commands (which implies it was a standalone unknown command)
            std::cerr << "Error: Unknown command '" << arg1 << "'." << std::endl;
            print_help();
            return ""; // Indicate error
        }
        // If arg1 was --help, --list-tags, or --curate, handle_cli_commands would have returned true,
        // and we wouldn't reach here.
    }
    return station_file;
}

// Runs the First-Run Wizard if the station_file (expected to be default) doesn't exist.
// Returns false if the wizard was run and cancelled/failed, true otherwise.
bool run_first_run_wizard_if_needed(const std::string& station_file) {
    std::ifstream f(station_file);
    if (!f.good()) { // File doesn't exist
        FirstRunWizard wizard;
        bool success = wizard.run();
        if (!success) {
            // Wizard's destructor calls endwin(), so std::cout is safe here.
            std::cout << "Setup cancelled. Exiting." << std::endl;
            return false;
        }
    }
    return true;
}

// Sets up and runs the main radio player.
void run_player(const std::string& station_file) {
    PersistenceManager persistence;
    StationData station_data = persistence.loadStations(station_file); // Can throw if file is invalid

    StationManager manager(station_data); // Can throw if station_data is empty
    RadioPlayer player(manager);
    player.run();
}

// --- Main Entry Point ---
int main(int argc, const char* argv[]) {
    // 1. Handle dedicated CLI commands that exit immediately (e.g., --help, --list-tags, --curate)
    if (handle_cli_commands(argc, argv)) {
        return 0; // CLI command was handled and app should exit (or already printed error).
    }

    // 2. Determine which station file to load (default or from --from)
    //    This also handles errors for unknown commands or misused --from.
    std::string station_file_to_load = determine_station_file(argc, argv);
    if (station_file_to_load.empty()) {
        return 1; // Error message and help already printed by determine_station_file.
    }

    // 3. Run the First-Run Wizard ONLY if we are trying to load the default "stations.jsonc"
    //    and it doesn't exist. If --from is used, we bypass the wizard.
    if (station_file_to_load == "stations.jsonc") {
        if (!run_first_run_wizard_if_needed(station_file_to_load)) {
            return 0; // Wizard was cancelled or failed.
        }
    }

    // 4. Suppress stderr for TUI mode and run the main player
    suppress_stderr();
    try {
        run_player(station_file_to_load);
    } catch (const std::exception& e) {
        log_critical_error(e);
        return 1;
    }

    return 0;
}
