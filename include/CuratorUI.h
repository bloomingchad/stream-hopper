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
              int discarded_count,
              const CuratorStation& station,
              const std::string& status,
              bool is_playing);

  private:
    void init_colors();
    void draw_progress_bar(int y, int x, int width, int current, int total);
    void draw_rating_stars(int votes);
    std::string truncate_string(const std::string& str, int max_width);
    void draw_tag_editor(int y, int x, const std::vector<std::string>& tags);
    void draw_quality_pill(int y, int x, int bitrate);
};

#endif // CURATORUI_H
