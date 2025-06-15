// src/RadioStream.cpp
#include "RadioStream.h"
#include "Utils.h"
#include <stdexcept>
#include <utility>

RadioStream::RadioStream(int id, std::string name, std::string url)
    : m_id(id), m_name(std::move(name)), m_url(std::move(url)), m_mpv(nullptr),
      m_is_initialized(false),
      m_current_title("..."),
      m_bitrate(0), m_is_muted(false), m_is_ducked(false),
      m_current_volume(0.0),
      m_pre_mute_volume(100.0), m_is_fading(false), m_target_volume(0.0), m_is_favorite(false),
      m_has_logged_first_song(false), m_is_buffering(false), 
      m_mute_start_time(std::nullopt) {}

RadioStream::~RadioStream() { destroy(); }

RadioStream::RadioStream(RadioStream &&other) noexcept
    : m_id(other.m_id), m_name(std::move(other.m_name)),
      m_url(std::move(other.m_url)), m_mpv(other.m_mpv),
      m_is_initialized(other.m_is_initialized.load()),
      m_current_title(other.getCurrentTitle()),
      m_bitrate(other.m_bitrate.load()),
      m_is_muted(other.m_is_muted.load()),
      m_is_ducked(other.m_is_ducked.load()),
      m_current_volume(other.m_current_volume.load()),
      m_pre_mute_volume(other.m_pre_mute_volume.load()),
      m_is_fading(other.m_is_fading.load()),
      m_target_volume(other.m_target_volume.load()),
      m_is_favorite(other.m_is_favorite.load()),
      m_has_logged_first_song(other.m_has_logged_first_song.load()),
      m_is_buffering(other.m_is_buffering.load()),
      m_mute_start_time(other.getMuteStartTime())
{
    other.m_mpv = nullptr;
}

RadioStream &RadioStream::operator=(RadioStream &&other) noexcept {
  if (this != &other) {
    destroy();
    m_id = other.m_id;
    m_name = std::move(other.m_name);
    m_url = std::move(other.m_url);
    m_mpv = other.m_mpv;
    m_is_initialized.store(other.m_is_initialized.load());
    setCurrentTitle(other.getCurrentTitle());
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
    
    {
        std::lock_guard<std::mutex> lock(m_mute_time_mutex);
        m_mute_start_time = other.getMuteStartTime();
    }

    other.m_mpv = nullptr;
  }
  return *this;
}

void RadioStream::initialize(double initial_volume) {
  if (m_is_initialized) return;

  m_mpv = mpv_create();
  if (!m_mpv) {
    throw std::runtime_error("Failed to create MPV instance for " + m_name);
  }
  
  // --- The Final, Definitive MPV Configuration Block ---
  // Synthesized from profiling, previous experience, and expert analysis.

  // Core Behavior: Isolate from system, disable all unneeded high-level features.
  mpv_set_option_string(m_mpv, "config", "no");
  mpv_set_option_string(m_mpv, "load-scripts", "no");
  mpv_set_option_string(m_mpv, "ytdl", "no");
  mpv_set_option_string(m_mpv, "input-default-bindings", "no");
  mpv_set_option_string(m_mpv, "input-media-keys", "no");

  // Resource Management: Aggressive memory and CPU conservation.
  mpv_set_option_string(m_mpv, "vo", "null");
  mpv_set_option_string(m_mpv, "hwdec", "no");
  mpv_set_option_string(m_mpv, "cache", "no");
  mpv_set_option_string(m_mpv, "demuxer-max-bytes", "1MiB");
  mpv_set_option_string(m_mpv, "demuxer-max-back-bytes", "1KiB"); // <-- ADDED: No back-buffer for live streams.
  mpv_set_option_string(m_mpv, "audio-buffer", "0.1");
  
  // Network Resilience & Behavior
  mpv_set_option_string(m_mpv, "timeout", "5");
  mpv_set_option_string(m_mpv, "demuxer-lavf-o", "reconnect=1,reconnect_streamed=1,reconnect_delay_max=5");
  
  // UI & Logging: Remain completely headless and silent.
  mpv_set_option_string(m_mpv, "terminal", "no");
  mpv_set_option_string(m_mpv, "msg-level", "all=error");
  // --- End Configuration Block ---

  check_mpv_error(mpv_initialize(m_mpv), "mpv_initialize for " + m_name);
  
  check_mpv_error(
      mpv_observe_property(m_mpv, m_id, "media-title", MPV_FORMAT_STRING),
      "observe media-title");
  check_mpv_error(
      mpv_observe_property(m_mpv, m_id, "audio-bitrate", MPV_FORMAT_INT64),
      "observe audio-bitrate");
  check_mpv_error(
      mpv_observe_property(m_mpv, m_id, "eof-reached", MPV_FORMAT_FLAG),
      "observe eof-reached");
  check_mpv_error(
      mpv_observe_property(m_mpv, m_id, "core-idle", MPV_FORMAT_FLAG),
      "observe core-idle");
  
  const char *cmd[] = {"loadfile", m_url.c_str(), "replace", nullptr};
  check_mpv_error(mpv_command_async(m_mpv, 0, cmd), "loadfile for " + m_name);
  
  m_current_volume = initial_volume;
  m_target_volume = initial_volume;
  
  mpv_set_property_async(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE, &initial_volume);
  
  m_is_initialized = true;
  setCurrentTitle("Initializing...");
}

void RadioStream::destroy() {
  if (!m_is_initialized) return;
  
  if (m_mpv) {
    mpv_terminate_destroy(m_mpv);
    m_mpv = nullptr;
  }
  m_is_initialized = false;
  setCurrentTitle("...");
}

bool RadioStream::isInitialized() const { return m_is_initialized; }

int RadioStream::getID() const { return m_id; }
const std::string &RadioStream::getName() const { return m_name; }
const std::string &RadioStream::getURL() const { return m_url; }
mpv_handle *RadioStream::getMpvHandle() const { return m_mpv; }

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
