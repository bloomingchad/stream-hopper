// include/RadioStream.h
#ifndef RADIOSTREAM_H
#define RADIOSTREAM_H

#include <string>
#include <mpv/client.h>
#include <atomic>
#include <mutex>
#include <vector>
#include "nlohmann/json_fwd.hpp"

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
    bool isFavorite() const;

    // Setters
    void setCurrentTitle(const std::string& title);
    void setMuted(bool muted);
    void setCurrentVolume(double vol);
    void setPreMuteVolume(double vol);
    void setFading(bool fading);
    void setTargetVolume(double vol);
    void toggleFavorite();
    
private:
    void destroy();
    
    int m_id;
    std::string m_name;
    std::string m_url;
    mpv_handle* m_mpv;
    
    std::string m_current_title;
    mutable std::mutex m_title_mutex;
    
    std::atomic<bool> m_is_muted;
    std::atomic<double> m_current_volume;
    std::atomic<double> m_pre_mute_volume;
    
    std::atomic<bool> m_is_fading;
    std::atomic<double> m_target_volume;
    
    std::atomic<bool> m_is_favorite{false};
};

#endif // RADIOSTREAM_H
