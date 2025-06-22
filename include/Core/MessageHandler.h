#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include "AppState.h" // For NavDirection

// Forward declaration to avoid circular dependencies
class StationManager;

class MessageHandler {
public:
    MessageHandler() = default;

    // A handler for each message type
    void handle_navigate(StationManager& manager, NavDirection direction);
    void handle_toggleMute(StationManager& manager);
    void handle_toggleAutoHop(StationManager& manager);
    void handle_toggleFavorite(StationManager& manager);
    void handle_toggleDucking(StationManager& manager);
    void handle_toggleCopyMode(StationManager& manager);
    void handle_toggleHopperMode(StationManager& manager);
    void handle_switchPanel(StationManager& manager);
    void handle_cycleUrl(StationManager& manager);
    void handle_updateAndPoll(StationManager& manager);
    void handle_quit(StationManager& manager);
};

#endif // MESSAGEHANDLER_H
