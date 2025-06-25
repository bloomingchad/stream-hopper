#include "FirstRunWizard.h"

#include <locale.h> // ** FIX: Include the necessary header for setlocale **
#include <ncurses.h>
#include <unistd.h> // for sleep

#include <algorithm>
#include <set>
#include <thread>

#include "CliHandler.h"
#include "CuratorStation.h"
#include "PersistenceManager.h"

namespace {
    // UI Constants
    constexpr int COLOR_PAIR_DEFAULT = 1;
    constexpr int COLOR_PAIR_HEADER = 2;
    constexpr int COLOR_PAIR_SELECTED = 3;
    constexpr int COLOR_PAIR_CURSOR = 4;
    constexpr int COLOR_PAIR_SUCCESS = 5;
    constexpr int COLOR_PAIR_INFO = 6;
} // namespace

FirstRunWizard::FirstRunWizard() { m_cli_handler = std::make_unique<CliHandler>(); }

FirstRunWizard::~FirstRunWizard() {
    if (stdscr != NULL && !isendwin()) {
        endwin();
    }
}

void FirstRunWizard::initialize_ui() {
    // ** FIX: Set the locale BEFORE initializing ncurses. **
    // This allows the library to correctly handle Unicode characters.
    setlocale(LC_ALL, "");

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100);

    start_color();
    use_default_colors();
    init_pair(COLOR_PAIR_DEFAULT, COLOR_WHITE, -1);
    init_pair(COLOR_PAIR_HEADER, COLOR_MAGENTA, -1);
    init_pair(COLOR_PAIR_SELECTED, COLOR_GREEN, -1);
    init_pair(COLOR_PAIR_CURSOR, COLOR_BLACK, COLOR_WHITE);
    init_pair(COLOR_PAIR_SUCCESS, COLOR_GREEN, -1);
    init_pair(COLOR_PAIR_INFO, COLOR_CYAN, -1);
}

bool FirstRunWizard::run() {
    initialize_ui();
    draw_message_screen("Welcome to Stream Hopper!", "Fetching available genres...", "", 2);

    m_available_genres = m_cli_handler->get_curated_tags();
    if (m_available_genres.empty()) {
        draw_message_screen("Error: Could not fetch genres.", "Please check your internet connection.",
                            "Exiting in 5 seconds...", 5);
        return false;
    }

    main_loop();

    if (!m_confirmed) {
        // FIX: Display helpful information on user cancellation.
        clear();
        int y = (LINES / 2) - 4;
        mvprintw(y, (COLS - 16) / 2, "Setup Cancelled.");
        y += 2;
        attron(COLOR_PAIR(COLOR_PAIR_INFO));
        mvprintw(y, (COLS - 40) / 2, "You can restart the wizard at any time.");
        y += 2;
        mvprintw(y, (COLS - 33) / 2, "To explore more options, try:");
        y += 1;
        mvprintw(y, (COLS - 33) / 2, "./build/stream-hopper --list-tags");
        y += 1;
        mvprintw(y, (COLS - 33) / 2, "./build/stream-hopper --curate <genre>");
        attroff(COLOR_PAIR(COLOR_PAIR_INFO));
        y += 2;
        mvprintw(y, (COLS - 20) / 2, "Exiting in 8 seconds...");
        refresh();
        std::this_thread::sleep_for(std::chrono::seconds(8));
        return false;
    }

    return perform_auto_curation();
}

void FirstRunWizard::main_loop() {
    while (!m_quit_flag) {
        draw();
        int ch = getch();
        if (ch != ERR) {
            handle_input(ch);
        }
    }
}

void FirstRunWizard::draw_message_screen(const std::string& line1, const std::string& line2, const std::string& line3,
                                         int delay_seconds) {
    clear();
    int y = (LINES / 2) - 2;
    int x1 = (COLS - line1.length()) / 2;
    int x2 = (COLS - line2.length()) / 2;
    int x3 = (COLS - line3.length()) / 2;

    attron(A_BOLD);
    mvprintw(y, x1, "%s", line1.c_str());
    attroff(A_BOLD);
    mvprintw(y + 2, x2, "%s", line2.c_str());
    mvprintw(y + 4, x3, "%s", line3.c_str());
    refresh();

    if (delay_seconds > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(delay_seconds));
    }
}

void FirstRunWizard::draw() {
    clear();
    int max_genres = GRID_COLS * GRID_ROWS;
    if (max_genres > (int)m_available_genres.size()) {
        max_genres = m_available_genres.size();
    }

    // Header
    attron(COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);
    mvprintw(1, 3, "Welcome to Stream Hopper! Let's pick your sound.");
    attroff(COLOR_PAIR(COLOR_PAIR_HEADER) | A_BOLD);

    // Instructions
    mvprintw(3, 3, "Use [ARROW KEYS] to navigate. [Space] to select. [Enter] to build your radio. [Q] to quit.");

    // Grid
    int col_width = (COLS - 4) / GRID_COLS;
    for (int i = 0; i < max_genres; ++i) {
        int row = i / GRID_COLS;
        int col = i % GRID_COLS;
        int y = 5 + (row * 2);
        int x = 3 + (col * col_width);

        bool is_selected = m_selected_indices.count(i);
        bool is_cursor = (col == m_cursor_x && row == m_cursor_y);

        std::string prefix = is_selected ? "[x] " : "[ ] ";

        if (is_cursor) {
            attron(COLOR_PAIR(COLOR_PAIR_CURSOR));
        } else if (is_selected) {
            attron(COLOR_PAIR(COLOR_PAIR_SELECTED) | A_BOLD);
        }

        mvprintw(y, x, "%s%s", prefix.c_str(), m_available_genres[i].c_str());

        if (is_cursor) {
            attroff(COLOR_PAIR(COLOR_PAIR_CURSOR));
        } else if (is_selected) {
            attroff(COLOR_PAIR(COLOR_PAIR_SELECTED) | A_BOLD);
        }
    }

    // Footer
    mvprintw(LINES - 2, 3, "Genres Selected: %zu", m_selected_indices.size());

    refresh();
}

void FirstRunWizard::handle_input(int ch) {
    int max_genres = GRID_COLS * GRID_ROWS;
     if (max_genres > (int)m_available_genres.size()) {
        max_genres = m_available_genres.size();
    }
    int max_rows = (max_genres + GRID_COLS - 1) / GRID_COLS;


    switch (ch) {
    case 'q':
    case 'Q':
        m_quit_flag = true;
        break;
    case KEY_UP:
        m_cursor_y = (m_cursor_y - 1 + max_rows) % max_rows;
        break;
    case KEY_DOWN:
        m_cursor_y = (m_cursor_y + 1) % max_rows;
        break;
    case KEY_LEFT:
        m_cursor_x = (m_cursor_x - 1 + GRID_COLS) % GRID_COLS;
        break;
    case KEY_RIGHT:
        m_cursor_x = (m_cursor_x + 1) % GRID_COLS;
        break;
    case ' ': {
        int current_index = m_cursor_y * GRID_COLS + m_cursor_x;
        if (current_index < max_genres) {
            if (m_selected_indices.count(current_index)) {
                m_selected_indices.erase(current_index);
            } else {
                m_selected_indices.insert(current_index);
            }
        }
        break;
    }
    case '\n': // Enter key
    case '\r':
        if (!m_selected_indices.empty()) {
            m_confirmed = true;
            m_quit_flag = true;
        }
        break;
    }

    // Boundary checks for cursor on irregular grids
    int current_index = m_cursor_y * GRID_COLS + m_cursor_x;
    if (current_index >= max_genres) {
        m_cursor_x = (max_genres - 1) % GRID_COLS;
        m_cursor_y = (max_genres - 1) / GRID_COLS;
    }
}

bool FirstRunWizard::perform_auto_curation() {
    std::vector<CuratorStation> final_stations;
    std::set<std::string> station_names; // For deduplication

    for (int index : m_selected_indices) {
        const std::string& genre = m_available_genres[index];
        draw_message_screen("Building your custom radio...", "Fetching stations for '" + genre + "'...");

        auto candidates = m_cli_handler->get_curation_candidates(genre);
        if (candidates.empty()) {
            continue;
        }

        // The script already sorted by votes, but we can do it again just to be safe.
        std::sort(candidates.begin(), candidates.end(),
                  [](const CuratorStation& a, const CuratorStation& b) { return a.votes > b.votes; });

        int added_count = 0;
        for (const auto& candidate : candidates) {
            if (added_count >= TOP_N_STATIONS_PER_GENRE) {
                break;
            }
            // Add if not already in our list
            if (station_names.find(candidate.name) == station_names.end()) {
                final_stations.push_back(candidate);
                station_names.insert(candidate.name);
                added_count++;
            }
        }
    }

    if (final_stations.empty()) {
        draw_message_screen("Auto-curation failed.", "No working stations found for selected genres.",
                            "Exiting in 5 seconds...", 5);
        return false;
    }

    PersistenceManager persistence;
    persistence.saveSimpleStationList("stations.jsonc", final_stations);

    draw_message_screen("✅ Radio Built Successfully!",
                        "You have " + std::to_string(final_stations.size()) + " stations ready to play.",
                        "Starting in 3 seconds...", 3);
    return true;
}
