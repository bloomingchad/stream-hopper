// include/UIManager.h
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

class UIManager {
public:
    UIManager();
    ~UIManager();

    void draw(const std::vector<RadioStream>& stations, const AppState& app_state,
              int remaining_seconds, int total_duration);

    int getInput();
    void setInputTimeout(int milliseconds);

private:
    void draw_compact_mode(int width, int height, const std::vector<RadioStream>& stations, const AppState& app_state,
                           int remaining_seconds, int total_duration);
    
    void draw_full_mode(int width, int height, const std::vector<RadioStream>& stations, const AppState& app_state,
                        int remaining_seconds, int total_duration);
    
    // UI Components
    std::unique_ptr<HeaderBar> m_header_bar;
    std::unique_ptr<FooterBar> m_footer_bar;
    std::unique_ptr<StationsPanel> m_stations_panel;
    std::unique_ptr<NowPlayingPanel> m_now_playing_panel;
    std::unique_ptr<HistoryPanel> m_history_panel;

    // Signal handling for resize
    static std::atomic<bool> s_resize_pending;
    static void handle_resize(int signum);
};

#endif // UIMANAGER_H
