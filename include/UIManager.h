// include/UIManager.h
#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <vector>
#include <string>
#include "nlohmann/json_fwd.hpp"

// Forward-declare RadioPlayer to get ActivePanel enum
class RadioPlayer; 
// This is a bit of a trick. To avoid a full circular dependency, we can
// forward-declare the class that holds the enum.
#include "RadioPlayer.h"


class RadioStream;

class UIManager {
public:
    UIManager();
    ~UIManager();

    // UPDATED: draw now takes the active panel and scroll offset
    void draw(const std::vector<RadioStream>& stations, int active_station_idx, 
              const nlohmann::json& history, ActivePanel active_panel, int scroll_offset);
    int getInput();

private:
    // Main layout drawers
    void draw_header_bar(int width, double current_volume);
    void draw_footer_bar(int y, int width, bool is_compact, bool is_small_mode);
    void draw_full_mode(int width, int height, const std::vector<RadioStream>& stations, int active_station_idx, 
                        const nlohmann::json& history, ActivePanel active_panel, int scroll_offset);
    void draw_compact_mode(int width, int height, const std::vector<RadioStream>& stations, int active_idx);

    // Panel content drawers (for full mode)
    void draw_stations_panel(int y, int x, int w, int h, const std::vector<RadioStream>& stations, 
                             int active_station_idx, bool is_focused);
    void draw_now_playing_panel(int y, int x, int w, int h, const RadioStream& station);
    void draw_history_panel(int y, int x, int w, int h, const RadioStream& station, 
                            const nlohmann::json& history, bool is_focused, int scroll_offset);

    // Generic drawing helper
    void draw_box(int y, int x, int w, int h, const std::string& title, bool is_focused);
};

#endif // UIMANAGER_H
