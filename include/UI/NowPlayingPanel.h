#ifndef NOWPLAYINGPANEL_H
#define NOWPLAYINGPANEL_H

#include "UI/Panel.h"
#include "nlohmann/json.hpp" // <-- FIX: Add the missing include here

// Forward declarations
struct StationDisplayData;

class NowPlayingPanel : public Panel {
public:
    void draw(const StationDisplayData& station, bool is_auto_hop_mode, int remaining_seconds, int total_duration);

private:
    void drawAutoHopView(int inner_w, int remaining_seconds, int total_duration);
    void drawNormalView(const StationDisplayData& station, int inner_w);
};

#endif // NOWPLAYINGPANEL_H
