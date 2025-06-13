// include/UIManager.h
#ifndef UIMANAGER_H
#define UIMANAGER_H

#include "nlohmann/json.hpp"
#include <vector>
#include <string>

// Forward declaration
class RadioStream;
enum class ActivePanel;

class UIManager {
public:
    UIManager();
    ~UIManager();

    void draw(const std::vector<RadioStream>& stations, int active_station_idx, 
              const nlohmann::json& history, ActivePanel active_panel, int scroll_offset, bool is_small_mode,
              int remaining_seconds, int total_duration);

    int getInput();

private:
    void draw_header_bar(int width, double current_volume);
    void draw_footer_bar(int y, int width, bool is_compact, bool is_small_mode);

    // *** THIS IS THE CHANGE ***
    // Add declaration for the new small mode drawing function.
    void draw_small_mode(int width, int height, const RadioStream& station, int remaining_seconds, int total_duration);

    void draw_compact_mode(int width, int height, const std::vector<RadioStream>& stations, int active_idx,
                           const nlohmann::json& history, ActivePanel active_panel, int scroll_offset);
    
    void draw_full_mode(int width, int height, const std::vector<RadioStream>& stations, int active_station_idx, 
                        const nlohmann::json& history, ActivePanel active_panel, int scroll_offset);
    
    void draw_stations_panel(int y, int x, int w, int h, const std::vector<RadioStream>& stations, int active_station_idx, bool is_focused);
    void draw_now_playing_panel(int y, int x, int w, int h, const RadioStream& station);
    void draw_history_panel(int y, int x, int w, int h, const RadioStream& station, 
                            const nlohmann::json& history, bool is_focused, int scroll_offset);
    void draw_box(int y, int x, int w, int h, const std::string& title, bool is_focused);

    int m_station_scroll_offset;
};

#endif // UIMANAGER_H
