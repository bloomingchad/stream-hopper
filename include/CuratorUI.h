#ifndef CURATORUI_H
#define CURATORUI_H

#include <string>

class CuratorUI {
  public:
    CuratorUI();
    ~CuratorUI();
    void draw(const std::string& genre,
              int current_index,
              int total_candidates,
              int kept_count,
              const std::string& station_name,
              const std::string& status);

  private:
    void init_colors();
};

#endif // CURATORUI_H
