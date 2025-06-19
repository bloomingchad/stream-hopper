#ifndef NOWPLAYINGPANEL_H
#define NOWPLAYINGPANEL_H

#include "UI/Panel.h"

// Forward declarations
class RadioStream;
class AppState;

class NowPlayingPanel : public Panel {
public:
    void draw(const RadioStream& station, bool is_auto_hop_mode, int remaining_seconds, int total_duration);

private:
    void drawAutoHopView(int inner_w, int remaining_seconds, int total_duration);
    void drawNormalView(const RadioStream& station, int inner_w);
};

#endif // NOWPLAYINGPANEL_H
