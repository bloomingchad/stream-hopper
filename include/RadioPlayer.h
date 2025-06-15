#ifndef RADIOPLAYER_H
#define RADIOPLAYER_H

#include <vector>
#include <string>
#include <memory>

#include "AppState.h" // Include new class

// Forward declarations
class UIManager;
class StationManager;

class RadioPlayer {
public:
    RadioPlayer(const std::vector<std::pair<std::string, std::string>>& station_data);
    ~RadioPlayer();

    void run();

private:
    void handleInput(int ch);
    void updateState();
    int getRemainingSecondsForCurrentStation();
    int getStationSwitchDuration();
    void toggleSmallMode();

    std::unique_ptr<UIManager> m_ui;
    std::unique_ptr<AppState> m_app_state;
    std::unique_ptr<StationManager> m_station_manager;
};

#endif // RADIOPLAYER_H
