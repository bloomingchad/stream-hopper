#ifndef COMPACTLAYOUTSTRATEGY_H
#define COMPACTLAYOUTSTRATEGY_H

#include <algorithm> // For std::max

#include "UI/Layout/ILayoutStrategy.h"

class CompactLayoutStrategy : public ILayoutStrategy {
  public:
    void calculateDimensions(int width,
                             int height,
                             HeaderBar& header,
                             FooterBar& footer,
                             StationsPanel& stations,
                             NowPlayingPanel& now_playing,
                             HistoryPanel& history,
                             const StateSnapshot& snapshot) override;
};

#endif // COMPACTLAYOUTSTRATEGY_H
