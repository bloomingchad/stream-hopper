#ifndef STATIONSPANEL_H
#define STATIONSPANEL_H

#include "UI/Panel.h"
#include <vector>
#include <string>

// Forward declarations
class RadioStream;
class AppState;

class StationsPanel : public Panel {
public:
    StationsPanel();
    void draw(const std::vector<RadioStream>& stations, const AppState& app_state, bool is_focused);

private:
    std::string getStationStatusString(const RadioStream& station) const;
    void drawStationLine(int y, const RadioStream& station, bool is_selected, int inner_w);

    int m_station_scroll_offset;
};

#endif // STATIONSPANEL_H
