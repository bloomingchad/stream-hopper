#include "UI/HeaderBar.h"

#include <ncurses.h>

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include "UI/UIUtils.h"

void HeaderBar::draw(double current_volume, HopperMode hopper_mode) {
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

    std::string full_header =
        " STREAM HOPPER  |  LIVE  |  " + mode_str + "  |  🔊 VOL: " + std::to_string((int) current_volume) + "% ";

    attron(A_REVERSE);
    mvprintw(m_y, m_x, "%s", std::string(m_w, ' ').c_str());
    mvprintw(m_y, m_x + 1, "%s", truncate_string(full_header, m_w - 2).c_str());
    attroff(A_REVERSE);
}
