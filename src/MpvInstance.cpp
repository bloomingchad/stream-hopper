#include "MpvInstance.h"
#include "Utils.h"
#include <stdexcept>
#include <utility>

MpvInstance::MpvInstance() : m_mpv(nullptr) {}

MpvInstance::~MpvInstance() {
    shutdown(); // Destructor now calls our new shutdown method
}

void MpvInstance::shutdown() {
    if (m_mpv) {
        // This is the ONLY place the handle should be destroyed.
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr; // Ensure handle is nulled after destruction
    }
}

MpvInstance::MpvInstance(MpvInstance&& other) noexcept 
    : m_mpv(other.m_mpv)
{
    other.m_mpv = nullptr; 
}

MpvInstance& MpvInstance::operator=(MpvInstance&& other) noexcept {
    if (this != &other) {
        shutdown(); // Use our new method
        m_mpv = other.m_mpv;
        other.m_mpv = nullptr;
    }
    return *this;
}

void MpvInstance::initialize(const std::string& url) {
    if (m_mpv) { // Already initialized, do nothing.
        return;
    }

    m_mpv = mpv_create();
    if (!m_mpv) {
        throw std::runtime_error("Failed to create MPV instance for url: " + url);
    }

    // This configuration block is moved from RadioStream into the RAII wrapper
    mpv_set_option_string(m_mpv, "config", "no");
    mpv_set_option_string(m_mpv, "load-scripts", "no");
    mpv_set_option_string(m_mpv, "ytdl", "no");
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "input-media-keys", "no");
    mpv_set_option_string(m_mpv, "vo", "null");
    mpv_set_option_string(m_mpv, "hwdec", "no");
    mpv_set_option_string(m_mpv, "cache", "no");
    mpv_set_option_string(m_mpv, "demuxer-max-bytes", "1MiB");
    mpv_set_option_string(m_mpv, "demuxer-max-back-bytes", "1KiB");
    mpv_set_option_string(m_mpv, "audio-buffer", "0.1");
    // Using a shorter timeout as reconnect logic is aggressive
    mpv_set_option_string(m_mpv, "timeout", "3"); 
    mpv_set_option_string(m_mpv, "demuxer-lavf-o", "reconnect=1,reconnect_streamed=1,reconnect_delay_max=4");
    mpv_set_option_string(m_mpv, "terminal", "no");
    mpv_set_option_string(m_mpv, "msg-level", "all=error");

    check_mpv_error(mpv_initialize(m_mpv), "mpv_initialize for " + url);
}

mpv_handle* MpvInstance::get() const {
    return m_mpv;
}
