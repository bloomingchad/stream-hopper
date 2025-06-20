#ifndef FOOTERBAR_H
#define FOOTERBAR_H

#include "UI/Panel.h"

class FooterBar : public Panel {
public:
    void draw(bool is_compact, bool is_copy_mode_active, bool is_auto_hop_mode_active);
};

#endif // FOOTERBAR_H
