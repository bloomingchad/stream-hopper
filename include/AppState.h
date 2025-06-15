#ifndef APPSTATE_H
#define APPSTATE_H

#include "nlohmann/json.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Forward declaration
class RadioStream;

enum class ActivePanel { STATIONS, HISTORY };

class AppState {
public:
  AppState();

  // State Variables
  std::atomic<int> active_station_idx;
  std::atomic<bool> quit_flag;
  std::atomic<bool> needs_redraw;
  std::atomic<bool> small_mode_active;
  std::atomic<bool> copy_mode_active;
  
  ActivePanel active_panel;
  int history_scroll_offset;
  std::chrono::steady_clock::time_point copy_mode_start_time;
  std::chrono::steady_clock::time_point small_mode_start_time;

  // History Management
  nlohmann::json& getHistory();
  void loadHistoryFromDisk();
  void saveHistoryToDisk();

  // Persistence (will be used by StationManager, but state is managed here)
  void loadFavorites(std::vector<RadioStream>& stations);
  void saveFavorites(const std::vector<RadioStream>& stations);
  void loadSession(const std::vector<RadioStream>& stations);
  void saveSession(const std::vector<RadioStream>& stations);

private:
  std::unique_ptr<nlohmann::json> m_song_history;
  std::mutex m_history_mutex;
};

#endif // APPSTATE_H
