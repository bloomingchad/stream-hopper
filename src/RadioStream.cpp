// src/RadioStream.cpp
#include "RadioStream.h"
#include "Utils.h"
#include <stdexcept>
#include <utility>

RadioStream::RadioStream(int id, std::string name, std::string url)
    : m_id(id), m_name(std::move(name)), m_url(std::move(url)), m_mpv(nullptr),
      m_current_title("Initializing..."), m_is_muted(false), m_current_volume(0.0),
      m_pre_mute_volume(100.0), m_is_fading(false), m_target_volume(0.0), m_is_favorite(false) {}

RadioStream::~RadioStream() { destroy(); }

RadioStream::RadioStream(RadioStream &&other) noexcept
    : m_id(other.m_id), m_name(std::move(other.m_name)),
      m_url(std::move(other.m_url)), m_mpv(other.m_mpv),
      m_current_title(other.getCurrentTitle()),
      m_is_muted(other.m_is_muted.load()),
      m_current_volume(other.m_current_volume.load()),
      m_pre_mute_volume(other.m_pre_mute_volume.load()),
      m_is_fading(other.m_is_fading.load()),
      m_target_volume(other.m_target_volume.load()),
      m_is_favorite(other.m_is_favorite.load())
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
    setCurrentTitle(other.getCurrentTitle());
    m_is_muted.store(other.m_is_muted.load());
    m_current_volume.store(other.m_current_volume.load());
    m_pre_mute_volume.store(other.m_pre_mute_volume.load());
    m_is_fading.store(other.m_is_fading.load());
    m_target_volume.store(other.m_target_volume.load());
    m_is_favorite.store(other.m_is_favorite.load());
    other.m_mpv = nullptr;
  }
  return *this;
}

void RadioStream::initialize(double initial_volume) {
  m_mpv = mpv_create();
  if (!m_mpv) {
    throw std::runtime_error("Failed to create MPV instance for " + m_name);
  }
  check_mpv_error(mpv_initialize(m_mpv), "mpv_initialize for " + m_name);
  mpv_set_property_string(m_mpv, "vo", "null");
  mpv_set_property_string(m_mpv, "audio-display", "no");
  mpv_set_property_string(m_mpv, "input-default-bindings", "no");
  mpv_set_property_string(m_mpv, "terminal", "no");
  mpv_set_property_string(m_mpv, "msg-level", "all=warn");
  check_mpv_error(
      mpv_observe_property(m_mpv, m_id, "media-title", MPV_FORMAT_STRING),
      "observe media-title");
  check_mpv_error(
      mpv_observe_property(m_mpv, m_id, "eof-reached", MPV_FORMAT_FLAG),
      "observe eof-reached");
  const char *cmd[] = {"loadfile", m_url.c_str(), "replace", nullptr};
  check_mpv_error(mpv_command_async(m_mpv, 0, cmd), "loadfile for " + m_name);
  
  m_current_volume = initial_volume;
  m_target_volume = initial_volume;
  if(initial_volume > 0) {
      m_is_muted = false;
  } else {
      m_is_muted = true;
  }
  mpv_set_property_async(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE, &initial_volume);
}

void RadioStream::destroy() {
  if (m_mpv) {
    mpv_terminate_destroy(m_mpv);
    m_mpv = nullptr;
  }
}

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

bool RadioStream::isMuted() const { return m_is_muted; }
void RadioStream::setMuted(bool muted) { m_is_muted = muted; }
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
