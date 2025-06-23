#ifndef HISTORYPANEL_H
#define HISTORYPANEL_H

#include "UI/Panel.h"
#include "nlohmann/json.hpp"

struct StationDisplayData;

class HistoryPanel : public Panel {
  public:
    void
    draw(const StationDisplayData& station, const nlohmann::json& station_history, int scroll_offset, bool is_focused);
};

#endif // HISTORYPANEL_H
