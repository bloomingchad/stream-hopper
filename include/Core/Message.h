#ifndef MESSAGE_H
#define MESSAGE_H

#include <string> // For using string in the message
#include <variant>

// This header is now the single source of truth for all message types.
// Any file that needs to know about messages can include this safely.

namespace Msg {
    // We no longer need a hardcoded enum. The key from the JSON is enough.
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
    // The message now carries the character key the user pressed.
    struct SearchOnline {
        char key;
    };
}

using StationManagerMessage = std::variant<Msg::NavigateUp,
                                           Msg::NavigateDown,
                                           Msg::ToggleMute,
                                           Msg::ToggleAutoHop,
                                           Msg::ToggleFavorite,
                                           Msg::ToggleDucking,
                                           Msg::ToggleCopyMode,
                                           Msg::ToggleHopperMode,
                                           Msg::SwitchPanel,
                                           Msg::CycleUrl,
                                           Msg::UpdateAndPoll,
                                           Msg::Quit,
                                           Msg::SearchOnline>;

#endif // MESSAGE_H
