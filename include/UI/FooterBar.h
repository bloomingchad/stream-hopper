#ifndef FOOTERBAR_H
#define FOOTERBAR_H

#include <string> // For std::string argument

#include "AppState.h"
#include "UI/Panel.h"

class FooterBar : public Panel {
  public:
    // Update signature to include temporary message
    void draw(AppMode app_mode,
              bool is_compact,
              bool is_copy_mode_active,
              bool is_auto_hop_mode_active,
              bool can_cycle_url,
              const std::string& temp_msg);
};

#endif // FOOTERBAR_H
