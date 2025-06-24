#ifndef CURATORAPP_H
#define CURATORAPP_H

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "CuratorStation.h" // Use the new rich data struct
#include "RadioStream.h"    // For RadioStream

class CuratorUI;

class CuratorApp {
  public:
    CuratorApp(const std::string& genre, std::vector<CuratorStation> candidates);
    ~CuratorApp();
    void run();

  private:
    void update_preloaded_stations();
    void advance(bool keep_current);
    void handle_input(int ch);
    void save_curated_list() const;

    std::string m_genre;
    std::vector<CuratorStation> m_candidates;
    std::vector<CuratorStation> m_kept_stations;
    int m_current_index;
    bool m_quit_flag;

    std::unique_ptr<CuratorUI> m_ui;
    std::deque<std::unique_ptr<RadioStream>> m_station_pool;
    static constexpr int PRELOAD_COUNT = 2;
};

#endif // CURATORAPP_H
