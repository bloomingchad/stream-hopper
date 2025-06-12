// include/UIManager.h
#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <vector>
#include <string>
#include "nlohmann/json_fwd.hpp" // Use forward declaration

// Forward declaration to avoid including the full RadioStream header here.
class RadioStream;

class UIManager {
public:
    UIManager();
    ~UIManager();

    void draw(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active, const nlohmann::json& history);
    int getInput();

private:
    // Main layout drawers
    // THIS IS THE FIX: Added the double parameter for volume.
    void draw_header_bar(int width, double current_volume);
    void draw_footer_bar(int y, int width);
    void draw_full_mode(int width, int height, const std::vector<RadioStream>& stations, int active_station_idx, const nlohmann::json& history);

    // Panel content drawers
    void draw_stations_panel(int y, int x, int w, int h, const std::vector<RadioStream>& stations, int active_station_idx);
    void draw_now_playing_panel(int y, int x, int w, int h, const RadioStream& station);
    void draw_history_panel(int y, int x, int w, int h, const RadioStream& station, const nlohmann::json& history);

    // Generic drawing helper
    void draw_box(int y, int x, int w, int h, const std::string& title);
};

#endif // UIMANAGER_H
