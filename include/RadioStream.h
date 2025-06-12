// include/RadioStream.h
#ifndef RADIOSTREAM_H
#define RADIOSTREAM_H

#include <string>
#include <mpv/client.h>
#include <atomic>
#include <mutex>
#include <vector>
#include "nlohmann/json_fwd.hpp" // Use forward declaration

class RadioStream {
public:
    RadioStream(int id, std::string name, std::string url);
    ~RadioStream();
    RadioStream(const RadioStream&) = delete;
    RadioStream& operator=(const RadioStream&) = delete;
    RadioStream(RadioStream&& other) noexcept;
    RadioStream& operator=(RadioStream&& other) noexcept;

    void initialize(double initial_volume);

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
    std::string getStatusString(bool is_active, bool is_small_mode) const;

    // Setters
    void setCurrentTitle(const std::string& title);
    void setMuted(bool muted);
    void setCurrentVolume(double vol);
    void setPreMuteVolume(double vol);
    void setFading(bool fading);
    void setTargetVolume(double vol);
    
private:
    void destroy();
    
    // Core properties
    int m_id;
    std::string m_name;
    std::string m_url;
    mpv_handle* m_mpv;
    
    // State properties
    std::string m_current_title;
    mutable std::mutex m_title_mutex;
    
    // Volume/Mute properties
    std::atomic<bool> m_is_muted;
    std::atomic<double> m_current_volume;
    std::atomic<double> m_pre_mute_volume;
    
    // Fading properties
    std::atomic<bool> m_is_fading;
    std::atomic<double> m_target_volume;
};

#endif // RADIOSTREAM_H
