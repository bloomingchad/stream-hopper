#ifndef MESSAGE_H
#define MESSAGE_H

#include <variant>

// This header is now the single source of truth for all message types.
// Any file that needs to know about messages can include this safely.

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

#endif // MESSAGE_H
