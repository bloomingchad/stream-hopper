#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <vector>
#include <string>
#include <atomic>
#include <memory>

// Forward declarations
class RadioStream;
class AppState;
class StationsPanel;
class NowPlayingPanel;
class HistoryPanel;
class HeaderBar;
class FooterBar;
class ILayoutStrategy;

class UIManager {
public:
    UIManager();
    ~UIManager();

    void draw(const std::vector<RadioStream>& stations, const AppState& app_state,
              int remaining_seconds, int total_duration);

    int getInput();
    void setInputTimeout(int milliseconds);

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

    // Signal handling for resize
    static std::atomic<bool> s_resize_pending;
    static void handle_resize(int signum);
};

#endif // UIMANAGER_H
