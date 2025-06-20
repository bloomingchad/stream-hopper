#ifndef HISTORYPANEL_H
#define HISTORYPANEL_H

#include "UI/Panel.h"

// Forward declarations
struct StationDisplayData;
class AppState;

class HistoryPanel : public Panel {
public:
    void draw(const StationDisplayData& station, const AppState& app_state, bool is_focused);
};

#endif // HISTORYPANEL_H
