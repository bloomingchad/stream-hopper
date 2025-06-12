// include/UIManager.h
#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <vector>
#include <string>

// Forward declaration to avoid including the full RadioStream header here.
class RadioStream;

class UIManager {
public:
    UIManager();
    ~UIManager();

    // The main draw function, updated for future steps.
    void draw(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active);
    int getInput();

private:
    void draw_header();
    
    // New function to draw a titled box.
    void draw_box(int y, int x, int w, int h, const std::string& title);
};

#endif // UIMANAGER_H
