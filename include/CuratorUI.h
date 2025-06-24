#ifndef CURATORUI_H
#define CURATORUI_H

#include <string>
#include <vector>

#include "CuratorStation.h" // For CuratorStation struct

class CuratorUI {
  public:
    CuratorUI();
    ~CuratorUI();

    void draw(const std::string& genre,
              int current_index,
              int total_candidates,
              int kept_count,
              const CuratorStation& station,
              const std::string& status);

    // New method for feedback animations
    void flash_feedback(bool is_keep);

  private:
    void init_colors();
    void draw_progress_bar(int y, int x, int width, int current, int total);
    std::string get_reliability_stars(int votes);
};

#endif // CURATORUI_H
