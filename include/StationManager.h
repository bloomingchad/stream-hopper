#ifndef STATIONMANAGER_H
#define STATIONMANAGER_H

#include <string>
#include <vector>
#include <thread> // Now using std::jthread
#include <memory>
#include <atomic>
#include <mutex> // For protecting the thread vector

#include "RadioStream.h"

// Forward declaration
class AppState;

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

private:
  void mpvEventLoop();
  void handleMpvEvent(struct mpv_event* event);
  
  void onTitleChanged(RadioStream& station, const std::string& new_title);
  void onStreamEof(RadioStream& station);

  void fadeAudio(RadioStream& station, double from_vol, double to_vol, int duration_ms);
  RadioStream* findStationById(int station_id);
  bool contains_ci(const std::string& haystack, const std::string& needle);

  void cleanupFinishedThreads(); // New private helper function

  std::vector<RadioStream> m_stations;
  AppState& m_app_state;
  std::thread m_mpv_event_thread;

  // --- NEW MEMBERS FOR MANAGED FADE THREADS ---
  std::vector<std::jthread> m_fade_threads;
  std::mutex m_fade_threads_mutex;
};

#endif // STATIONMANAGER_H
