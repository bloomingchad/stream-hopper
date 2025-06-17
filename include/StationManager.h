#ifndef STATIONMANAGER_H
#define STATIONMANAGER_H

#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <mutex> // For protecting the thread vector

#include "RadioStream.h"

// Forward declaration
class AppState;
struct mpv_event;
struct mpv_event_property;

class StationManager {
public:
  StationManager(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data, AppState& app_state);
  ~StationManager();

  void runEventLoop();
  void stopEventLoop();

  // High-level actions
  void switchStation(int old_idx, int new_idx);
  void toggleMuteStation(int station_idx);
  void toggleAudioDucking(int station_idx);
  void toggleFavorite(int station_idx);
  
  // Getters for UI
  const std::vector<RadioStream>& getStations() const;

  // --- NEW: Wakeup Mechanism ---
  // Wakes up the event loop thread to process a state change.
  void wakeupEventLoop();

private:
  void mpvEventLoop();
  
  // --- NEW: MPV Event Handler Helpers ---
  void handleMpvEvent(mpv_event* event);
  void handlePropertyChange(mpv_event* event);
  void onTitleProperty(mpv_event_property* prop, RadioStream& station);
  void onBitrateProperty(mpv_event_property* prop, RadioStream& station);
  void onEofProperty(mpv_event_property* prop, RadioStream& station);
  void onCoreIdleProperty(mpv_event_property* prop, RadioStream& station);

  // --- Existing Helpers ---
  void onTitleChanged(RadioStream& station, const std::string& new_title);
  void onStreamEof(RadioStream& station);
  void fadeAudio(RadioStream& station, double from_vol, double to_vol, int duration_ms);
  RadioStream* findStationById(int station_id);
  bool contains_ci(const std::string& haystack, const std::string& needle);
  void cleanupFinishedThreads();

  std::vector<RadioStream> m_stations;
  AppState& m_app_state;
  std::thread m_mpv_event_thread;

  // --- NEW: Event Loop Synchronization ---
  std::mutex m_event_mutex;
  std::condition_variable m_event_cond;
  std::atomic<bool> m_wakeup_flag;
  static void mpv_wakeup_cb(void* ctx);

  std::vector<std::thread> m_fade_threads;
  std::mutex m_fade_threads_mutex;
};

#endif // STATIONMANAGER_H
