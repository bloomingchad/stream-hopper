#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

// Forward declaration to avoid circular dependencies
class StationManager;

class UpdateManager {
public:
    UpdateManager() = default;

    // The main entry point for processing all time-based updates
    void process_updates(StationManager& manager);

private:
    // Private helpers for each specific update task
    void handle_activeFades(StationManager& manager);
    void handle_cycle_status_timers(StationManager& manager);
    void handle_cycle_timeouts(StationManager& manager);
    void handle_temporary_message_timer(StationManager& manager); // New handler
};

#endif // UPDATEMANAGER_H
