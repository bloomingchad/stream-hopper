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
};

#endif // SYSTEMHANDLER_H
