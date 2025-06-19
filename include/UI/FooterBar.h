#ifndef FOOTERBAR_H
#define FOOTERBAR_H

#include "UI/Panel.h"

// Forward declarations
class AppState;

class FooterBar : public Panel {
public:
    void draw(bool is_compact, const AppState& app_state);
};

#endif // FOOTERBAR_H
