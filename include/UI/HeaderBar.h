#ifndef HEADERBAR_H
#define HEADERBAR_H

#include "UI/Panel.h"
#include "AppState.h" // For HopperMode enum

class HeaderBar : public Panel {
public:
    void draw(double current_volume, HopperMode hopper_mode);
};

#endif // HEADERBAR_H
