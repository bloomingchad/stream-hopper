#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <atomic>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
class StationsPanel;
class NowPlayingPanel;
class HistoryPanel;
class HeaderBar;
class FooterBar;
class ILayoutStrategy;
struct StateSnapshot; // The one and only data source

class UIManager {
  public:
    UIManager();
    ~UIManager();

    // The signature is now much simpler. It only needs the snapshot.
    void draw(const StateSnapshot& snapshot);

    int getInput();
    void setInputTimeout(int milliseconds);
    void handleResize();

  private:
    void updateLayoutStrategy(int width);

    // UI Components
    std::unique_ptr<HeaderBar> m_header_bar;
    std::unique_ptr<FooterBar> m_footer_bar;
    std::unique_ptr<StationsPanel> m_stations_panel;
    std::unique_ptr<NowPlayingPanel> m_now_playing_panel;
    std::unique_ptr<HistoryPanel> m_history_panel;

    // Layout Strategy
    std::unique_ptr<ILayoutStrategy> m_layout_strategy;
    bool m_is_compact_mode;

    // Signal handling for resize is now instance-based
    static std::atomic<bool> s_resize_pending;
    static void resize_handler_trampoline(int signum);
};

#endif // UIMANAGER_H
