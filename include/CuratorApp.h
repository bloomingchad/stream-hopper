#ifndef CURATORAPP_H
#define CURATORAPP_H

#include <memory>
#include <string>
#include <vector>

#include "PersistenceManager.h" // For StationData

class CuratorUI;

class CuratorApp {
  public:
    CuratorApp(const std::string& genre, StationData candidates);
    ~CuratorApp();
    void run();

  private:
    std::string m_genre;
    StationData m_candidates;
    StationData m_kept_stations;
    int m_current_index;
    bool m_quit_flag;

    std::unique_ptr<CuratorUI> m_ui;
};

#endif // CURATORAPP_H
