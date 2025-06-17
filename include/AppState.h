#ifndef APPSTATE_H
#define APPSTATE_H

#include "nlohmann/json.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <deque> // For navigation history

// Forward declaration
class RadioStream;

enum class NavDirection { UP, DOWN }; // For tracking navigation

struct NavEvent {
    NavDirection direction;
    std::chrono::steady_clock::time_point timestamp;
};

enum class HopperMode {
    BALANCED,     // 🍃 Default: Windowed pre-loading
    PERFORMANCE,  // 🚀 All stations loaded
    FOCUS         // 🎧 Only active station loaded
};
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
  HopperMode hopper_mode;
  std::chrono::steady_clock::time_point last_switch_time;
  
  // --- NEW: Navigation History for Acceleration Detection ---
  std::deque<NavEvent> nav_history;

  // --- Session Statistics ---
  std::chrono::steady_clock::time_point session_start_time;
  std::atomic<int> session_switches;
  std::atomic<int> new_songs_found;
  std::atomic<int> songs_copied;

  // --- THREAD-SAFE HISTORY MANAGEMENT ---
  void addHistoryEntry(const std::string& station_name, const nlohmann::json& entry);
  nlohmann::json getStationHistory(const std::string& station_name) const;
  size_t getStationHistorySize(const std::string& station_name) const;
  void ensureStationHistoryExists(const std::string& station_name);
  void loadHistoryFromDisk();
  void saveHistoryToDisk();

  // Persistence (will be used by StationManager, but state is managed here)
  void loadFavorites(std::vector<RadioStream>& stations);
  void saveFavorites(const std::vector<RadioStream>& stations);
  void loadSession(const std::vector<RadioStream>& stations);
  void saveSession(const std::vector<RadioStream>& stations);

private:
  nlohmann::json& getHistory(); // Now private
  std::unique_ptr<nlohmann::json> m_song_history;
  mutable std::mutex m_history_mutex; // Made mutable to be used in const methods
};

#endif // APPSTATE_H
