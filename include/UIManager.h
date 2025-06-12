// include/UIManager.h
#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <vector>
#include <string>
#include "nlohmann/json_fwd.hpp"

class RadioStream;

class UIManager {
public:
    UIManager();
    ~UIManager();

    void draw(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active, const nlohmann::json& history);
    int getInput();

private:
    // Main layout drawers
    void draw_header_bar(int width, double current_volume);
    void draw_footer_bar(int y, int width, bool is_compact); // Added bool flag
    void draw_full_mode(int width, int height, const std::vector<RadioStream>& stations, int active_station_idx, const nlohmann::json& history);
    
    // NEW: Compact mode layout drawer
    void draw_compact_mode(int width, int height, const std::vector<RadioStream>& stations, int active_station_idx);

    // Panel content drawers (for full mode)
    void draw_stations_panel(int y, int x, int w, int h, const std::vector<RadioStream>& stations, int active_station_idx);
    void draw_now_playing_panel(int y, int x, int w, int h, const RadioStream& station);
    void draw_history_panel(int y, int x, int w, int h, const RadioStream& station, const nlohmann::json& history);

    // Generic drawing helper
    void draw_box(int y, int x, int w, int h, const std::string& title);
};

#endif // UIMANAGER_H
