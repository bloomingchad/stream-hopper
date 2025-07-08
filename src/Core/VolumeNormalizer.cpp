#include "Core/VolumeNormalizer.h"

#include <algorithm>

#include "RadioStream.h"
#include "StationManager.h" // To call applyCombinedVolume

void VolumeNormalizer::adjust(StationManager& manager, RadioStream& station, double amount) {
    double current_offset = station.getVolumeOffset();
    double new_offset = std::clamp(current_offset + amount, MIN_OFFSET, MAX_OFFSET);

    station.setVolumeOffset(new_offset);

    // Activate the UI and set its timeout
    m_is_ui_active = true;
    m_ui_timeout_end = std::chrono::steady_clock::now() + std::chrono::seconds(UI_TIMEOUT_SECONDS);

    // Tell the station manager to apply the new, final volume to mpv
    manager.applyCombinedVolume(station.getID());
}

bool VolumeNormalizer::checkTimeout() {
    if (m_is_ui_active && m_ui_timeout_end) {
        if (std::chrono::steady_clock::now() >= *m_ui_timeout_end) {
            m_is_ui_active = false;
            m_ui_timeout_end.reset();
            return true; // Return true to signal that a save should be triggered
        }
    }
    return false;
}

bool VolumeNormalizer::isUiActive() const { return m_is_ui_active; }
