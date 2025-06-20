#ifndef ILAYOUTSTRATEGY_H
#define ILAYOUTSTRATEGY_H

// Forward declarations reduce header coupling
class HeaderBar;
class FooterBar;
class StationsPanel;
class NowPlayingPanel;
class HistoryPanel;
class AppState;

class ILayoutStrategy {
public:
    virtual ~ILayoutStrategy() = default;

    virtual void calculateDimensions(
        int width, int height,
        HeaderBar& header, FooterBar& footer,
        StationsPanel& stations, NowPlayingPanel& now_playing, HistoryPanel& history,
        const AppState& app_state
    ) = 0;
};

#endif // ILAYOUTSTRATEGY_H
