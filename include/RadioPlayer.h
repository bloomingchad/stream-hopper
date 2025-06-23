#ifndef RADIOPLAYER_H
#define RADIOPLAYER_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class UIManager;
class StationManager;
#include "Core/Message.h" // Include the new message header

class RadioPlayer {
  public:
    // Now takes a reference to the already-created manager
    RadioPlayer(StationManager& manager);
    ~RadioPlayer();

    void run();

  private:
    void handleInput(int ch);

    std::map<int, StationManagerMessage> m_input_handlers;
    std::unique_ptr<UIManager> m_ui;
    // StationManager is now the owner of all state, we just talk to it.
    StationManager& m_station_manager;
};

#endif // RADIOPLAYER_H
