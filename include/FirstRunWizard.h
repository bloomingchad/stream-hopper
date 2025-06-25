#ifndef FIRSTRUNWIZARD_H
#define FIRSTRUNWIZARD_H

#include <memory>
#include <set>
#include <string>
#include <vector>

class CliHandler;

class FirstRunWizard {
  public:
    FirstRunWizard();
    ~FirstRunWizard();

    // Runs the entire wizard UI and logic. Returns true if a station
    // file was successfully created, false if the user cancelled.
    bool run();

  private:
    void initialize_ui();
    void main_loop();
    void draw();
    void handle_input(int ch);
    bool perform_auto_curation();
    void draw_message_screen(const std::string& line1, const std::string& line2 = "", const std::string& line3 = "",
                             int delay_seconds = 0);

    std::unique_ptr<CliHandler> m_cli_handler;
    std::vector<std::string> m_available_genres;
    std::set<int> m_selected_indices;

    // UI State
    int m_cursor_x = 0;
    int m_cursor_y = 0;
    bool m_quit_flag = false;
    bool m_confirmed = false;

    // Constants
    static constexpr int GRID_COLS = 3;
    static constexpr int GRID_ROWS = 5;
    static constexpr int TOP_N_STATIONS_PER_GENRE = 5;
};

#endif // FIRSTRUNWIZARD_H
