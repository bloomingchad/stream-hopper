#ifndef CURATORUI_H
#define CURATORUI_H

#include <string>
#include <vector>

#include "CuratorStation.h"

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

  private:
    void init_colors();
    void draw_progress_bar(int y, int x, int width, int current, int total);
    std::string get_reliability_stars(int votes);
    std::string truncate_string(const std::string& str, int max_width); // ✅ Added helper
};

#endif // CURATORUI_H
