#ifndef STATIONSPANEL_H
#define STATIONSPANEL_H

#include "UI/Panel.h"
#include <vector>

// Forward declarations
class RadioStream;
class AppState;

class StationsPanel : public Panel {
public:
    StationsPanel();
    void draw(const std::vector<RadioStream>& stations, const AppState& app_state, bool is_focused);

private:
    int m_station_scroll_offset;
};

#endif // STATIONSPANEL_H
