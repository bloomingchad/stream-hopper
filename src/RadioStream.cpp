#include "RadioStream.h"
#include "Utils.h"
#include <stdexcept>
#include <utility>
#include <mpv/client.h>

RadioStream::RadioStream(int id, std::string name, std::string url)
    : m_id(id), m_name(std::move(name)), m_url(std::move(url)), m_mpv_instance(),
      m_is_initialized(false),
      m_generation(0),
      m_current_title("..."), m_bitrate(0),
      m_playback_state(PlaybackState::Playing),
      m_current_volume(0.0), 
      m_pre_mute_volume(100.0), m_is_fading(false), m_target_volume(0.0), m_is_favorite(false),
      m_has_logged_first_song(false), m_is_buffering(false), 
      m_mute_start_time(std::nullopt) {}

RadioStream::RadioStream(RadioStream&& other) noexcept
    : m_id(other.m_id),
      m_name(std::move(other.m_name)),
      m_url(std::move(other.m_url)),
      m_mpv_instance(std::move(other.m_mpv_instance)),
      m_is_initialized(other.m_is_initialized),
      m_generation(other.m_generation),
      m_current_title(std::move(other.m_current_title)),
      m_bitrate(other.m_bitrate),
      m_playback_state(other.m_playback_state),
      m_current_volume(other.m_current_volume),
      m_pre_mute_volume(other.m_pre_mute_volume),
      m_is_fading(other.m_is_fading),
      m_target_volume(other.m_target_volume),
      m_is_favorite(other.m_is_favorite),
      m_has_logged_first_song(other.m_has_logged_first_song),
      m_is_buffering(other.m_is_buffering),
      m_mute_start_time(std::move(other.m_mute_start_time))
{}

RadioStream& RadioStream::operator=(RadioStream&& other) noexcept
{
    if (this != &other) {
        m_id = other.m_id;
        m_name = std::move(other.m_name);
        m_url = std::move(other.m_url);
        m_mpv_instance = std::move(other.m_mpv_instance);
        
        m_is_initialized = other.m_is_initialized;
        m_generation = other.m_generation;
        m_current_title = std::move(other.m_current_title);
        m_bitrate = other.m_bitrate;
        m_playback_state = other.m_playback_state;
        m_current_volume = other.m_current_volume;
        m_pre_mute_volume = other.m_pre_mute_volume;
        m_is_fading = other.m_is_fading;
        m_target_volume = other.m_target_volume;
        m_is_favorite = other.m_is_favorite;
        m_has_logged_first_song = other.m_has_logged_first_song;
        m_is_buffering = other.m_is_buffering;
        m_mute_start_time = std::move(other.m_mute_start_time);
    }
    return *this;
}

void RadioStream::shutdown() {
    if (!m_is_initialized) return;

    // Increment the generation. This immediately invalidates all
    // existing fade threads for this station.
    m_generation++; 
    
    // Now destroy the instance. The destructor of MpvInstance handles it.
    m_mpv_instance = MpvInstance{};

    // Reset all state variables AFTER destruction.
    m_is_initialized = false;
    setCurrentTitle("...");
    m_bitrate = 0;
    m_playback_state = PlaybackState::Playing;
    m_current_volume = 0.0;
    m_is_fading = false;
    m_is_buffering = false;
    m_has_logged_first_song = false;
}

void RadioStream::initialize(double initial_volume) {
  if (m_is_initialized) return;

  m_mpv_instance.initialize(m_url);
  
  mpv_handle* mpv = m_mpv_instance.get();
  if (!mpv) {
      throw std::runtime_error("MpvInstance failed to provide a valid handle for " + m_name);
  }
  
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
int RadioStream::getGeneration() const { return m_generation; }

int RadioStream::getID() const { return m_id; }
const std::string &RadioStream::getName() const { return m_name; }
const std::string &RadioStream::getURL() const { return m_url; }
mpv_handle *RadioStream::getMpvHandle() const { return m_mpv_instance.get(); }

std::string RadioStream::getCurrentTitle() const {
  return m_current_title;
}

void RadioStream::setCurrentTitle(const std::string &title) {
  m_current_title = title;
}

int RadioStream::getBitrate() const { return m_bitrate; }
void RadioStream::setBitrate(int bitrate) { m_bitrate = bitrate; }

PlaybackState RadioStream::getPlaybackState() const { return m_playback_state; }
void RadioStream::setPlaybackState(PlaybackState state) { m_playback_state = state; }

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
    return m_mute_start_time;
}

void RadioStream::setMuteStartTime() {
    m_mute_start_time = std::chrono::steady_clock::now();
}

void RadioStream::resetMuteStartTime() {
    m_mute_start_time = std::nullopt;
}
