#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include "AppState.h" // For NavDirection
#include <variant> // For StationManagerMessage definition

// Forward declare StationManager to break the include cycle
class StationManager;

// --- Message Definitions ---
// This now becomes the single source of truth for the message types.
namespace Msg {
    struct NavigateUp {};
    struct NavigateDown {};
    struct ToggleMute {};
    struct ToggleAutoHop {};
    struct ToggleFavorite {};
    struct ToggleDucking {};
    struct ToggleCopyMode {};
    struct ToggleHopperMode {};
    struct SwitchPanel {};
    struct CycleUrl {};
    struct UpdateAndPoll {}; 
    struct Quit {};
}

using StationManagerMessage = std::variant<
    Msg::NavigateUp, Msg::NavigateDown, Msg::ToggleMute, Msg::ToggleAutoHop,
    Msg::ToggleFavorite, Msg::ToggleDucking, Msg::ToggleCopyMode,
    Msg::ToggleHopperMode, Msg::SwitchPanel, Msg::CycleUrl, Msg::UpdateAndPoll, Msg::Quit
>;
// --- End Message Definitions ---


class MessageHandler {
public:
    MessageHandler() = default;

    // The main entry point for dispatching any message
    void process_message(StationManager& manager, const StationManagerMessage& msg);

private:
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
