#ifndef STATIONMANAGER_H
#define STATIONMANAGER_H

#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <unordered_set>
#include <deque>
#include <future> 

#include "RadioStream.h"
#include "AppState.h" 

struct mpv_event;
struct mpv_event_property;

class StationManager {
public:
  StationManager(const std::vector<std::pair<std::string, std::vector<std::string>>>& station_data, AppState& app_state);
  ~StationManager();

  void runEventLoop();
  void stopEventLoop();

  void switchStation(int old_idx, int new_idx);
  void toggleMuteStation(int station_idx);
  void toggleAudioDucking(int station_idx);
  void toggleFavorite(int station_idx);
  
  void setHopperMode(HopperMode new_mode);
  void updateActiveWindow();

  const std::vector<RadioStream>& getStations() const;

  void wakeupEventLoop();

private:
  void mpvEventLoop();
  
  void handleMpvEvent(mpv_event* event);
  void handlePropertyChange(mpv_event* event);
  void onTitleProperty(mpv_event_property* prop, RadioStream& station);
  void onBitrateProperty(mpv_event_property* prop, RadioStream& station);
  void onEofProperty(mpv_event_property* prop, RadioStream& station);
  void onCoreIdleProperty(mpv_event_property* prop, RadioStream& station);

  void onTitleChanged(RadioStream& station, const std::string& new_title);
  void onStreamEof(RadioStream& station);
  void fadeAudio(RadioStream& station, double from_vol, double to_vol, int duration_ms);
  RadioStream* findStationById(int station_id);
  bool contains_ci(const std::string& haystack, const std::string& needle);
  void cleanupFinishedFutures();
  
  // --- Request-based Lifecycle Management ---
  void requestStationInitialization(int station_idx);
  void requestStationShutdown(int station_idx);
  void processLifecycleRequests();

  std::vector<RadioStream> m_stations;
  AppState& m_app_state;
  std::unordered_set<int> m_active_station_indices;
  std::thread m_mpv_event_thread;

  std::mutex m_active_indices_mutex; 

  std::mutex m_event_mutex;
  std::condition_variable m_event_cond;
  std::atomic<bool> m_wakeup_flag;
  static void mpv_wakeup_cb(void* ctx);

  std::vector<std::future<void>> m_fade_futures;
  std::mutex m_fade_futures_mutex;
  
  // --- Lifecycle Queues ---
  std::deque<int> m_initialization_queue;
  std::deque<int> m_shutdown_queue;
  std::mutex m_lifecycle_queue_mutex;
};

#endif // STATIONMANAGER_H
