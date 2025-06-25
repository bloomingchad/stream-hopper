#ifndef CURATORAPP_H
#define CURATORAPP_H

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "CuratorStation.h"
#include "RadioStream.h"

class CuratorUI;

class CuratorApp {
  public:
    CuratorApp(const std::string& genre, std::vector<CuratorStation> candidates);
    ~CuratorApp();
    void run();

  private:
    void update_preloaded_stations();
    void advance(bool keep_current);
    void go_back();
    void handle_input(int ch);
    // void edit_tags(); // Will be renamed/become part of handle_edit_tags_action
    void save_curated_list() const;

    // Helpers for run()
    void process_mpv_events_for_pool();
    std::string get_active_station_status_string() const;
    CuratorStation get_station_display_data() const;

    // New private helper methods for handle_input()
    void handle_quit_action();
    void handle_keep_action();
    void handle_discard_action();
    void handle_back_action();
    void handle_edit_tags_action();
    void handle_play_toggle_action();

    std::string m_genre;
    std::vector<CuratorStation> m_candidates;
    std::vector<CuratorStation> m_kept_stations;
    std::deque<int> m_history; // Track navigation history
    int m_current_index;
    bool m_quit_flag;
    int m_discarded_count = 0;
    bool m_is_active_station_playing = true;

    std::unique_ptr<CuratorUI> m_ui;
    std::deque<std::unique_ptr<RadioStream>> m_station_pool;
    static constexpr int PRELOAD_COUNT = 2;
};

#endif // CURATORAPP_H
