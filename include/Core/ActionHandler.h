#ifndef ACTIONHANDLER_H
#define ACTIONHANDLER_H

#include "AppState.h"
#include "Core/Message.h" // Include the new message header

class StationManager;

// Handles all direct user actions
class ActionHandler {
public:
    void process_action(StationManager& manager, const StationManagerMessage& msg);

private:
    void handle_navigateStations(StationManager& manager, NavDirection direction);
    void handle_navigateHistory(StationManager& manager, NavDirection direction);
    void handle_navigate(StationManager& manager, NavDirection direction);
    void handle_toggleMute(StationManager& manager);
    void handle_toggleAutoHop(StationManager& manager);
    void handle_toggleFavorite(StationManager& manager);
    void handle_toggleDucking(StationManager& manager);
    void handle_toggleCopyMode(StationManager& manager);
    void handle_toggleHopperMode(StationManager& manager);
    void handle_switchPanel(StationManager& manager);
    void handle_cycleUrl(StationManager& manager);
    void handle_searchOnline(StationManager& manager, char key);
};

#endif // ACTIONHANDLER_H
