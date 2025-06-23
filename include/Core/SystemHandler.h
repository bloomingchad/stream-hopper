#ifndef SYSTEMHANDLER_H
#define SYSTEMHANDLER_H

#include "Core/Message.h" // Include the new message header

class StationManager;

// Handles system-level and lifecycle messages
class SystemHandler {
public:
    void process_system(StationManager& manager, const StationManagerMessage& msg);

private:
    void handle_updateAndPoll(StationManager& manager);
    void handle_quit(StationManager& manager);

    // Private helpers for each timer-based check
    void check_copy_mode_timeout(StationManager& manager);
    void check_auto_hop_timer(StationManager& manager);
    void check_focus_mode_timer(StationManager& manager);
    void check_mute_timeout(StationManager& manager);
};

#endif // SYSTEMHANDLER_H
