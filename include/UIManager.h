// include/UIManager.h
#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <vector>
#include <string>

// Forward declaration
class RadioStream;
class AppState;

class UIManager {
public:
    UIManager();
    ~UIManager();

    void draw(const std::vector<RadioStream>& stations, const AppState& app_state,
              int remaining_seconds, int total_duration);

    int getInput();
    void setInputTimeout(int milliseconds);

private:
    void draw_header_bar(int width, double current_volume, const AppState& app_state);

    void draw_footer_bar(int y, int width, bool is_compact, const AppState& app_state);

    void draw_compact_mode(int width, int height, const std::vector<RadioStream>& stations, const AppState& app_state,
                           int remaining_seconds, int total_duration);
    
    void draw_full_mode(int width, int height, const std::vector<RadioStream>& stations, const AppState& app_state,
                        int remaining_seconds, int total_duration);
    
    void draw_stations_panel(int y, int x, int w, int h, const std::vector<RadioStream>& stations, 
                             const AppState& app_state, bool is_focused);
    
    void draw_now_playing_panel(int y, int x, int w, int h, const RadioStream& station, bool is_small_mode,
                                int remaining_seconds, int total_duration);
    
    void draw_history_panel(int y, int x, int w, int h, const RadioStream& station, 
                            const AppState& app_state, bool is_focused);
                            
    void draw_box(int y, int x, int w, int h, const std::string& title, bool is_focused);

    mutable int m_station_scroll_offset;
};

#endif // UIMANAGER_H
