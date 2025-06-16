// src/RadioStream.cpp
#include "RadioStream.h"
#include "Utils.h"
#include <stdexcept>
#include <utility>
#include <mpv/client.h> // Needed for mpv_... functions

RadioStream::RadioStream(int id, std::string name, std::string url)
    : m_id(id), m_name(std::move(name)), m_url(std::move(url)), m_mpv_instance(),
      m_is_initialized(false),
      m_current_title("..."),
      m_bitrate(0), m_is_muted(false), m_is_ducked(false),
      m_current_volume(0.0),
      m_pre_mute_volume(100.0), m_is_fading(false), m_target_volume(0.0), m_is_favorite(false),
      m_has_logged_first_song(false), m_is_buffering(false), 
      m_mute_start_time(std::nullopt) {}

RadioStream::RadioStream(RadioStream&& other) noexcept
    : m_id(other.m_id),
      m_name(std::move(other.m_name)),
      m_url(std::move(other.m_url)),
      m_mpv_instance(std::move(other.m_mpv_instance)),
      m_is_initialized(other.m_is_initialized.load()),
      m_bitrate(other.m_bitrate.load()),
      m_is_muted(other.m_is_muted.load()),
      m_is_ducked(other.m_is_ducked.load()),
      m_current_volume(other.m_current_volume.load()),
      m_pre_mute_volume(other.m_pre_mute_volume.load()),
      m_is_fading(other.m_is_fading.load()),
      m_target_volume(other.m_target_volume.load()),
      m_is_favorite(other.m_is_favorite.load()),
      m_has_logged_first_song(other.m_has_logged_first_song.load()),
      m_is_buffering(other.m_is_buffering.load())
{
    // Manually move state protected by mutexes
    std::lock_guard<std::mutex> other_title_lock(other.m_title_mutex);
    m_current_title = std::move(other.m_current_title);
    
    std::lock_guard<std::mutex> other_mute_time_lock(other.m_mute_time_mutex);
    m_mute_start_time = std::move(other.m_mute_start_time);
}

RadioStream& RadioStream::operator=(RadioStream&& other) noexcept
{
    if (this != &other) {
        // Lock both sets of mutexes to prevent deadlocks
        std::scoped_lock lock(m_title_mutex, other.m_title_mutex, m_mute_time_mutex, other.m_mute_time_mutex);

        // Move all members
        m_id = other.m_id;
        m_name = std::move(other.m_name);
        m_url = std::move(other.m_url);
        m_mpv_instance = std::move(other.m_mpv_instance);
        
        m_is_initialized.store(other.m_is_initialized.load());
        m_current_title = std::move(other.m_current_title);
        m_bitrate.store(other.m_bitrate.load());
        m_is_muted.store(other.m_is_muted.load());
        m_is_ducked.store(other.m_is_ducked.load());
        m_current_volume.store(other.m_current_volume.load());
        m_pre_mute_volume.store(other.m_pre_mute_volume.load());
        m_is_fading.store(other.m_is_fading.load());
        m_target_volume.store(other.m_target_volume.load());
        m_is_favorite.store(other.m_is_favorite.load());
        m_has_logged_first_song.store(other.m_has_logged_first_song.load());
        m_is_buffering.store(other.m_is_buffering.load());
        m_mute_start_time = std::move(other.m_mute_start_time);
    }
    return *this;
}

void RadioStream::initialize(double initial_volume) {
  if (m_is_initialized) return;

  m_mpv_instance.initialize(m_url);
  
  mpv_handle* mpv = m_mpv_instance.get();
  if (!mpv) {
      throw std::runtime_error("MpvInstance failed to provide a valid handle for " + m_name);
  }
  
  // The MPV configuration block is now handled by MpvInstance::initialize().
  
  check_mpv_error(
      mpv_observe_property(mpv, m_id, "media-title", MPV_FORMAT_STRING),
      "observe media-title");
  check_mpv_error(
      mpv_observe_property(mpv, m_id, "audio-bitrate", MPV_FORMAT_INT64),
      "observe audio-bitrate");
  check_mpv_error(
      mpv_observe_property(mpv, m_id, "eof-reached", MPV_FORMAT_FLAG),
      "observe eof-reached");
  check_mpv_error(
      mpv_observe_property(mpv, m_id, "core-idle", MPV_FORMAT_FLAG),
      "observe core-idle");
  
  const char *cmd[] = {"loadfile", m_url.c_str(), "replace", nullptr};
  check_mpv_error(mpv_command_async(mpv, 0, cmd), "loadfile for " + m_name);
  
  m_current_volume = initial_volume;
  m_target_volume = initial_volume;
  
  mpv_set_property_async(mpv, 0, "volume", MPV_FORMAT_DOUBLE, &initial_volume);
  
  m_is_initialized = true;
  setCurrentTitle("Initializing...");
}

bool RadioStream::isInitialized() const { return m_is_initialized; }

int RadioStream::getID() const { return m_id; }
const std::string &RadioStream::getName() const { return m_name; }
const std::string &RadioStream::getURL() const { return m_url; }
mpv_handle *RadioStream::getMpvHandle() const { return m_mpv_instance.get(); }

std::string RadioStream::getCurrentTitle() const {
  std::lock_guard<std::mutex> lock(m_title_mutex);
  return m_current_title;
}

void RadioStream::setCurrentTitle(const std::string &title) {
  std::lock_guard<std::mutex> lock(m_title_mutex);
  m_current_title = title;
}

int RadioStream::getBitrate() const { return m_bitrate; }
void RadioStream::setBitrate(int bitrate) { m_bitrate = bitrate; }

bool RadioStream::isMuted() const { return m_is_muted; }
void RadioStream::setMuted(bool muted) { m_is_muted = muted; }

bool RadioStream::isDucked() const { return m_is_ducked; }
void RadioStream::setDucked(bool ducked) { m_is_ducked = ducked; }

double RadioStream::getCurrentVolume() const { return m_current_volume; }
void RadioStream::setCurrentVolume(double vol) { m_current_volume = vol; }
double RadioStream::getPreMuteVolume() const { return m_pre_mute_volume; }
void RadioStream::setPreMuteVolume(double vol) { m_pre_mute_volume = vol; }
bool RadioStream::isFading() const { return m_is_fading; }
void RadioStream::setFading(bool fading) { m_is_fading = fading; }
double RadioStream::getTargetVolume() const { return m_target_volume; }
void RadioStream::setTargetVolume(double vol) { m_target_volume = vol; }
bool RadioStream::isFavorite() const { return m_is_favorite; }
void RadioStream::toggleFavorite() { m_is_favorite = !m_is_favorite; }

bool RadioStream::hasLoggedFirstSong() const { return m_has_logged_first_song; }
void RadioStream::setHasLoggedFirstSong(bool has_logged) { m_has_logged_first_song = has_logged; }

bool RadioStream::isBuffering() const { return m_is_buffering; }
void RadioStream::setBuffering(bool buffering) { m_is_buffering = buffering; }

std::optional<std::chrono::steady_clock::time_point> RadioStream::getMuteStartTime() const {
    std::lock_guard<std::mutex> lock(m_mute_time_mutex);
    return m_mute_start_time;
}

void RadioStream::setMuteStartTime() {
    std::lock_guard<std::mutex> lock(m_mute_time_mutex);
    m_mute_start_time = std::chrono::steady_clock::now();
}

void RadioStream::resetMuteStartTime() {
    std::lock_guard<std::mutex> lock(m_mute_time_mutex);
    m_mute_start_time = std::nullopt;
}
