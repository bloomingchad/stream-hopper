#ifndef HEADERBAR_H
#define HEADERBAR_H

#include "UI/Panel.h"

// Forward declarations
class AppState;

class HeaderBar : public Panel {
public:
    void draw(double current_volume, const AppState& app_state);
};

#endif // HEADERBAR_H
