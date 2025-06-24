#ifndef CURATORAPP_H
#define CURATORAPP_H

#include <memory>
#include <string>
#include <vector>

#include "PersistenceManager.h" // For StationData
#include "RadioStream.h"        // For RadioStream

class CuratorUI;

class CuratorApp {
  public:
    CuratorApp(const std::string& genre, StationData candidates);
    ~CuratorApp();
    void run();

  private:
    void load_current_station();
    void handle_input(int ch);
    void advance(bool keep_current);
    void save_curated_list() const;

    std::string m_genre;
    StationData m_candidates;
    StationData m_kept_stations;
    int m_current_index;
    bool m_quit_flag;

    std::unique_ptr<CuratorUI> m_ui;
    std::unique_ptr<RadioStream> m_active_station;
    mpv_handle* m_mpv_ctx; // Shared mpv context for event handling
};

#endif // CURATORAPP_H
