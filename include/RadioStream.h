// include/RadioStream.h
#ifndef RADIOSTREAM_H
#define RADIOSTREAM_H

#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
#include <optional>
#include "MpvInstance.h" // New RAII wrapper

// Forward declare to avoid including mpv/client.h in this header
struct mpv_handle;

class RadioStream {
public:
    RadioStream(int id, std::string name, std::string url);
    
    // Rule of 5: Manually define move operations due to non-movable members
    // (std::atomic, std::mutex). Let compiler handle destructor.
    ~RadioStream() = default;
    RadioStream(const RadioStream&) = delete;
    RadioStream& operator=(const RadioStream&) = delete;
    RadioStream(RadioStream&& other) noexcept;
    RadioStream& operator=(RadioStream&& other) noexcept;

    void initialize(double initial_volume);

    bool isInitialized() const;

    int getID() const;
    const std::string& getName() const;
    const std::string& getURL() const;
    mpv_handle* getMpvHandle() const;

    std::string getCurrentTitle() const;
    void setCurrentTitle(const std::string& title);

    int getBitrate() const;
    void setBitrate(int bitrate);

    bool isMuted() const;
    void setMuted(bool muted);

    bool isDucked() const;
    void setDucked(bool ducked);

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
    MpvInstance m_mpv_instance; // Replaces raw mpv_handle*
    
    std::atomic<bool> m_is_initialized;

    mutable std::mutex m_title_mutex;
    std::string m_current_title;

    std::atomic<int> m_bitrate;
    std::atomic<bool> m_is_muted;
    std::atomic<bool> m_is_ducked;
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
