#ifndef STATIONMANAGER_H
#define STATIONMANAGER_H

#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>

#include "RadioStream.h"

// Forward declaration
class AppState;

class StationManager {
public:
  StationManager(const std::vector<std::pair<std::string, std::string>>& station_data, AppState& app_state);
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

  std::vector<RadioStream> m_stations;
  AppState& m_app_state;
  std::thread m_mpv_event_thread;
};

#endif // STATIONMANAGER_H
