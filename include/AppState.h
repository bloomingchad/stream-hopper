#ifndef APPSTATE_H
#define APPSTATE_H

#include <chrono>
#include <deque>

// This header is now just for shared, simple type definitions (enums)
// to avoid circular dependencies between the components that use them.

enum class NavDirection {
    UP,
    DOWN
};

struct NavEvent {
    NavDirection direction;
    std::chrono::steady_clock::time_point timestamp;
};

enum class HopperMode {
    BALANCED,
    PERFORMANCE,
    FOCUS
};

enum class ActivePanel {
    STATIONS,
    HISTORY
};

#endif // APPSTATE_H
