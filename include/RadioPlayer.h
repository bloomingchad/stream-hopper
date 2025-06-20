#ifndef RADIOPLAYER_H
#define RADIOPLAYER_H

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <functional>
#include "AppState.h" 

class UIManager;
class StationManager;

class RadioPlayer {
public:
    RadioPlayer(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data);
    ~RadioPlayer();

    void run();

private:
    void handleInput(int ch);
    void updateState();
    int getRemainingSecondsForCurrentStation() const;
    int getStationSwitchDuration(size_t station_count) const;

    void onUpArrow();
    void onDownArrow();
    void onEnter();
    void onToggleAutoHopMode();
    void onToggleFavorite();
    void onToggleDucking();
    void onCopyMode();
    void onToggleHopperMode();
    void onQuit();
    void onSwitchPanel();

    std::map<int, std::function<void()>> m_input_handlers;
    std::unique_ptr<UIManager> m_ui;
    std::unique_ptr<AppState> m_app_state;
    std::unique_ptr<StationManager> m_station_manager;
};

#endif // RADIOPLAYER_H
