#ifndef NOWPLAYINGPANEL_H
#define NOWPLAYINGPANEL_H

#include "UI/Panel.h"
#include "UI/StateSnapshot.h"
#include "nlohmann/json.hpp"

class NowPlayingPanel : public Panel {
  public:
    void draw(const StateSnapshot& snapshot);

  private:
    void drawAutoHopView(int inner_w, int remaining_seconds, int total_duration);
    void drawNormalView(const StationDisplayData& station, int inner_w);
    void drawVolumeOffsetBar(const StationDisplayData& station, int inner_w);
    void drawCycleStatus(const StationDisplayData& station, int inner_w);
};

#endif // NOWPLAYINGPANEL_H
