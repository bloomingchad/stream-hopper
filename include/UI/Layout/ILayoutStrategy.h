#ifndef ILAYOUTSTRATEGY_H
#define ILAYOUTSTRATEGY_H

// Forward declarations reduce header coupling
class HeaderBar;
class FooterBar;
class StationsPanel;
class NowPlayingPanel;
class HistoryPanel;
struct StateSnapshot;

class ILayoutStrategy {
  public:
    virtual ~ILayoutStrategy() = default;

    virtual void calculateDimensions(int width,
                                     int height,
                                     HeaderBar& header,
                                     FooterBar& footer,
                                     StationsPanel& stations,
                                     NowPlayingPanel& now_playing,
                                     HistoryPanel& history,
                                     const StateSnapshot& snapshot) = 0;
};

#endif // ILAYOUTSTRATEGY_H
