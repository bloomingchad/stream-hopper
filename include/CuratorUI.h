#ifndef CURATORUI_H
#define CURATORUI_H

#include <string>

#include "CuratorStation.h" // For CuratorStation struct

class CuratorUI {
  public:
    CuratorUI();
    ~CuratorUI();
    // <<< FIX: Updated signature to take the whole CuratorStation object
    void draw(const std::string& genre,
              int current_index,
              int total_candidates,
              int kept_count,
              const CuratorStation& station,
              const std::string& status);

  private:
    void init_colors();
};

#endif // CURATORUI_H
