// UIManager.h
#pragma once

#include <vector>
#include <string>
#include "RadioStream.h" // Needs the full definition of RadioStream

class UIManager {
public:
    UIManager();
    ~UIManager();

    void draw(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active, int remaining_seconds);
    int getInput();

private:
    void draw_header(bool small_mode_active, int remaining_seconds);
    void draw_station_list(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active);
    void draw_footer(bool small_mode_active);
};
