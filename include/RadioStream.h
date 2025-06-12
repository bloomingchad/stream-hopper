// RadioStream.h
#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <mpv/client.h>

class RadioStream {
public:
    // Constructor and Destructor
    RadioStream(int id, std::string name, std::string url);
    ~RadioStream();

    // Rule of five: Disable copy, provide move
    RadioStream(const RadioStream&) = delete;
    RadioStream& operator=(const RadioStream&) = delete;
    RadioStream(RadioStream&& other) noexcept;
    RadioStream& operator=(RadioStream&& other) noexcept;

    // Public methods
    void initialize(double initial_volume);
    void destroy();
    std::string getStatusString(bool is_active, bool is_small_mode = false) const;

    // Getters
    int getID() const;
    const std::string& getName() const;
    const std::string& getURL() const;
    mpv_handle* getMpvHandle() const;
    std::string getCurrentTitle() const;
    bool isMuted() const;
    double getCurrentVolume() const;
    double getPreMuteVolume() const;
    bool isFading() const;
    double getTargetVolume() const;

    // Setters
    void setCurrentTitle(const std::string& title);
    void setMuted(bool muted);
    void setCurrentVolume(double vol);
    void setPreMuteVolume(double vol);
    void setFading(bool fading);
    void setTargetVolume(double vol);

private:
    int m_id;
    std::string m_name;
    std::string m_url;
    mpv_handle* m_mpv;
    std::string m_current_title;
    mutable std::mutex m_title_mutex;
    std::atomic<bool> m_is_fading;
    std::atomic<double> m_target_volume;
    std::atomic<double> m_current_volume;
    std::atomic<bool> m_is_muted;
    std::atomic<double> m_pre_mute_volume;
};
