#include "UI/HeaderBar.h"

#include <ncurses.h>

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "UI/UIUtils.h"

namespace {
    const std::vector<char> SPINNER_CHARS = {'/', '-', '\\', '|'};
    int spinner_idx = 0;
}

void HeaderBar::draw(double current_volume, HopperMode hopper_mode, AppMode app_mode, bool is_fetching) {
    std::string mode_str;
    switch (hopper_mode) {
    case HopperMode::BALANCED:
        mode_str = "🍃 Balanced";
        break;
    case HopperMode::PERFORMANCE:
        mode_str = "🚀 Performance";
        break;
    case HopperMode::FOCUS:
        mode_str = "🎧 Focus";
        break;
    }

    std::string play_mode_str = (app_mode == AppMode::RANDOM) ? "RANDOM" : "LIVE";
    if (is_fetching) {
        spinner_idx = (spinner_idx + 1) % SPINNER_CHARS.size();
        play_mode_str += " ";
        play_mode_str += SPINNER_CHARS[spinner_idx];
    }

    std::string full_header = " STREAM HOPPER  |  " + play_mode_str +
                              "  |  " + mode_str + "  |  🔊 VOL: " + std::to_string((int) current_volume) + "% ";

    attron(A_REVERSE);
    mvprintw(m_y, m_x, "%s", std::string(m_w, ' ').c_str());
    mvprintw(m_y, m_x + 1, "%s", truncate_string(full_header, m_w - 2).c_str());
    attroff(A_REVERSE);
}
