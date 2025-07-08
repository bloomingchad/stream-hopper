#ifndef VOLUMENORMALIZER_H
#define VOLUMENORMALIZER_H

#include <chrono>
#include <optional>

class RadioStream;
class StationManager;

class VolumeNormalizer {
  public:
    // --- Public Constants for UI and Logic ---
    static constexpr double ADJUSTMENT_STEP = 1.0;
    static constexpr double MAX_OFFSET = 40.0;
    static constexpr double MIN_OFFSET = -40.0;

    VolumeNormalizer() = default;

    // Activates the UI, updates the station's offset, and applies the new combined volume.
    void adjust(StationManager& manager, RadioStream& station, double amount);

    // Checks if the UI timeout has been reached.
    // Returns true if the state changed from active to inactive, signaling a save is needed.
    bool checkTimeout();

    bool isUiActive() const;

  private:
    bool m_is_ui_active = false;
    std::optional<std::chrono::steady_clock::time_point> m_ui_timeout_end;

    static constexpr int UI_TIMEOUT_SECONDS = 4;
};

#endif // VOLUMENORMALIZER_H
