#ifndef HEADERBAR_H
#define HEADERBAR_H

#include "AppState.h" // For HopperMode enum
#include "UI/Panel.h"

class HeaderBar : public Panel {
  public:
    void draw(double current_volume, HopperMode hopper_mode);
};

#endif // HEADERBAR_H
