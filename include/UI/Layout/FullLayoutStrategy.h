#ifndef FULLLAYOUTSTRATEGY_H
#define FULLLAYOUTSTRATEGY_H

#include "UI/Layout/ILayoutStrategy.h"
#include <algorithm> // For std::max

class FullLayoutStrategy : public ILayoutStrategy {
public:
    void calculateDimensions(
        int width, int height,
        HeaderBar& header, FooterBar& footer,
        StationsPanel& stations, NowPlayingPanel& now_playing, HistoryPanel& history,
        const StateSnapshot& snapshot
    ) override;
};

#endif // FULLLAYOUTSTRATEGY_H
