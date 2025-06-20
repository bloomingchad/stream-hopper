#ifndef STATIONSPANEL_H
#define STATIONSPANEL_H

#include "UI/Panel.h"
#include <vector>
#include <string>

// Forward declarations
struct StationDisplayData;

class StationsPanel : public Panel {
public:
    StationsPanel();
    void draw(const std::vector<StationDisplayData>& stations, int active_station_idx, bool is_focused);

private:
    std::string getStationStatusString(const StationDisplayData& station) const;
    void drawStationLine(int y, const StationDisplayData& station, bool is_selected, int inner_w);

    int m_station_scroll_offset;
};

#endif // STATIONSPANEL_H
