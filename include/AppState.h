#ifndef APPSTATE_H
#define APPSTATE_H

#include "nlohmann/json.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <deque>

class RadioStream;

enum class NavDirection { UP, DOWN };

struct NavEvent {
    NavDirection direction;
    std::chrono::steady_clock::time_point timestamp;
};

enum class HopperMode {
    BALANCED,
    PERFORMANCE,
    FOCUS
};
enum class ActivePanel { STATIONS, HISTORY };

class AppState {
public:
  AppState();

  // State Variables
  std::atomic<int> active_station_idx;
  std::atomic<bool> quit_flag;
  std::atomic<bool> needs_redraw;
  std::atomic<bool> auto_hop_mode_active;
  std::atomic<bool> copy_mode_active;
  
  ActivePanel active_panel;
  int history_scroll_offset;
  std::chrono::steady_clock::time_point copy_mode_start_time;
  std::chrono::steady_clock::time_point auto_hop_start_time;
  HopperMode hopper_mode;
  std::chrono::steady_clock::time_point last_switch_time;
  
  std::deque<NavEvent> nav_history;

  // --- Session Statistics ---
  std::chrono::steady_clock::time_point session_start_time;
  std::atomic<int> session_switches;
  std::atomic<int> new_songs_found;
  std::atomic<int> songs_copied;
  std::atomic<int> unsaved_history_count;

  // --- THREAD-SAFE IN-MEMORY HISTORY MANAGEMENT ---
  void setHistory(nlohmann::json&& history_data);
  void addHistoryEntry(const std::string& station_name, const nlohmann::json& entry);
  nlohmann::json getStationHistory(const std::string& station_name) const;
  size_t getStationHistorySize(const std::string& station_name) const;
  void ensureStationHistoryExists(const std::string& station_name);
  nlohmann::json getFullHistory() const;

private:
  nlohmann::json& getHistory();
  std::unique_ptr<nlohmann::json> m_song_history;
  mutable std::mutex m_history_mutex;
};

#endif // APPSTATE_H
