#ifndef RADIOSTREAM_H
#define RADIOSTREAM_H

#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
#include <optional>
#include "MpvInstance.h"

struct mpv_handle;

enum class PlaybackState { Playing, Muted, Ducked };

class RadioStream {
public:
    RadioStream(int id, std::string name, std::string url);
    
    ~RadioStream() = default;
    RadioStream(const RadioStream&) = delete;
    RadioStream& operator=(const RadioStream&) = delete;
    RadioStream(RadioStream&& other) noexcept;
    RadioStream& operator=(RadioStream&& other) noexcept;

    void initialize(double initial_volume);
    void shutdown();

    bool isInitialized() const;
    int getGeneration() const; // <-- NEW

    int getID() const;
    const std::string& getName() const;
    const std::string& getURL() const;
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

private:
    int m_id;
    std::string m_name;
    std::string m_url;
    MpvInstance m_mpv_instance;
    
    std::atomic<bool> m_is_initialized;
    std::atomic<int> m_generation; // <-- NEW: The "kill switch" counter

    mutable std::mutex m_title_mutex;
    std::string m_current_title;

    std::atomic<int> m_bitrate;
    std::atomic<PlaybackState> m_playback_state;
    std::atomic<double> m_current_volume;
    std::atomic<double> m_pre_mute_volume;
    std::atomic<bool> m_is_fading;
    std::atomic<double> m_target_volume;
    std::atomic<bool> m_is_favorite;
    std::atomic<bool> m_has_logged_first_song;
    std::atomic<bool> m_is_buffering;
    
    std::optional<std::chrono::steady_clock::time_point> m_mute_start_time;
    mutable std::mutex m_mute_time_mutex;
};

#endif // RADIOSTREAM_H
