#ifndef RADIOSTREAM_H
#define RADIOSTREAM_H

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "MpvInstance.h"

struct mpv_handle;

enum class PlaybackState {
    Playing,
    Muted,
    Ducked
};
enum class CyclingState {
    IDLE,
    CYCLING,
    SUCCEEDED,
    FAILED
};

class RadioStream {
  public:
    RadioStream(int id, std::string name, std::vector<std::string> urls);

    ~RadioStream() = default;
    RadioStream(const RadioStream&) = delete;
    RadioStream& operator=(const RadioStream&) = delete;
    RadioStream(RadioStream&& other) noexcept;
    RadioStream& operator=(RadioStream&& other) noexcept;

    void initialize(double initial_volume);
    void shutdown();

    // --- URL Cycling Methods & State ---
    void startCycle();
    void finalizeCycle(bool success);
    void clearCycleStatus();
    void setPendingTitle(const std::string& title);
    void setPendingBitrate(int bitrate);
    void promotePendingMetadata();
    void promotePendingToActive();
    CyclingState getCyclingState() const;
    std::chrono::steady_clock::time_point getCycleStatusEndTime() const;
    const std::string& getNextUrl() const;
    MpvInstance& getPendingMpvInstance();
    const std::string& getPendingTitle() const;
    int getPendingBitrate() const;
    std::optional<std::chrono::steady_clock::time_point> getCycleStartTime() const;
    // ------------------------------------

    bool isInitialized() const;
    int getGeneration() const;
    int getID() const;
    const std::string& getName() const;
    const std::string& getActiveUrl() const;
    const std::vector<std::string>& getAllUrls() const;
    size_t getActiveUrlIndex() const;
    mpv_handle* getMpvHandle() const;

    std::string getCurrentTitle() const;
    void setCurrentTitle(const std::string& title);
    int getBitrate() const;
    void setBitrate(int bitrate);
    PlaybackState getPlaybackState() const;
    void setPlaybackState(PlaybackState state);
    double getCurrentVolume() const;
    void setCurrentVolume(double vol);
    double getPreMuteVolume() const;
    void setPreMuteVolume(double vol);
    bool isFading() const;
    void setFading(bool fading);
    double getTargetVolume() const;
    void setTargetVolume(double vol);
    bool isFavorite() const;
    void toggleFavorite();
    bool hasLoggedFirstSong() const;
    void setHasLoggedFirstSong(bool has_logged);
    bool isBuffering() const;
    void setBuffering(bool buffering);
    std::optional<std::chrono::steady_clock::time_point> getMuteStartTime() const;
    void setMuteStartTime();
    void resetMuteStartTime();

    // --- Volume Normalization ---
    double getVolumeOffset() const;
    void setVolumeOffset(double offset);

  private:
    int m_id;
    std::string m_name;
    std::vector<std::string> m_urls;
    size_t m_active_url_index;
    MpvInstance m_mpv_instance;
    MpvInstance m_pending_mpv_instance;
    bool m_is_initialized;
    int m_generation;
    CyclingState m_cycling_state;
    std::chrono::steady_clock::time_point m_cycle_status_end_time;

    std::string m_pending_title;
    int m_pending_bitrate;
    std::optional<std::chrono::steady_clock::time_point> m_cycle_start_time;

    std::string m_current_title;
    int m_bitrate;
    PlaybackState m_playback_state;
    double m_current_volume;
    double m_pre_mute_volume;
    bool m_is_fading;
    double m_target_volume;
    bool m_is_favorite;
    bool m_has_logged_first_song;
    bool m_is_buffering;
    std::optional<std::chrono::steady_clock::time_point> m_mute_start_time;
    double m_volume_offset;
};

#endif // RADIOSTREAM_H
