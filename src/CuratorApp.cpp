#include "CuratorApp.h"

#include <ncurses.h>

#include <fstream>
#include <iomanip>
#include <thread> // for sleep_for

#include "CuratorUI.h"
#include "Utils.h" // For check_mpv_error
#include "nlohmann/json.hpp"

using json = nlohmann::json;

CuratorApp::CuratorApp(const std::string& genre, StationData candidates)
    : m_genre(genre),
      m_candidates(std::move(candidates)),
      m_current_index(0),
      m_quit_flag(false),
      m_mpv_ctx(nullptr) // mpv_ctx is not needed for this simple case.
{
    m_ui = std::make_unique<CuratorUI>();
    if (!m_candidates.empty()) {
        load_current_station();
    } else {
        m_quit_flag = true; // No candidates, nothing to do.
    }
}

CuratorApp::~CuratorApp() {
    // RadioStream destructor handles its own mpv instance shutdown.
}

void CuratorApp::load_current_station() {
    // The RadioStream's destructor will automatically call shutdown().
    m_active_station = nullptr;

    if (m_current_index >= (int) m_candidates.size()) {
        m_quit_flag = true; // Finished all candidates
        return;
    }

    const auto& candidate = m_candidates[m_current_index];
    // Create a new RadioStream. ID 0 is fine as there's only one at a time.
    m_active_station = std::make_unique<RadioStream>(0, candidate.first, candidate.second);
    m_active_station->initialize(100.0); // Start at full volume
}

void CuratorApp::advance(bool keep_current) {
    if (m_current_index < (int)m_candidates.size() && keep_current) {
        m_kept_stations.push_back(m_candidates[m_current_index]);
    }

    m_current_index++;

    if (m_current_index >= (int) m_candidates.size()) {
        m_quit_flag = true;
        return;
    }
    load_current_station();
}

void CuratorApp::handle_input(int ch) {
    switch (ch) {
    case 'q':
    case 'Q':
        m_quit_flag = true;
        break;
    case 'k':
    case 'K':
        advance(true);
        break;
    case 'd':
    case 'D':
        advance(false);
        break;
    case 'p':
    case 'P':
        if (m_active_station && m_active_station->isInitialized()) {
            if (m_active_station->getPlaybackState() == PlaybackState::Muted) {
                m_active_station->setPlaybackState(PlaybackState::Playing);
                double vol = 100.0;
                mpv_set_property(m_active_station->getMpvHandle(), "volume", MPV_FORMAT_DOUBLE, &vol);
            } else {
                m_active_station->setPlaybackState(PlaybackState::Muted);
                double vol = 0.0;
                mpv_set_property(m_active_station->getMpvHandle(), "volume", MPV_FORMAT_DOUBLE, &vol);
            }
        }
        break;
    }
}

void CuratorApp::run() {
    while (!m_quit_flag) {
        // Poll mpv properties to update the UI state.
        if (m_active_station && m_active_station->isInitialized()) {
            char* title_cstr = nullptr;
            if (mpv_get_property(m_active_station->getMpvHandle(), "media-title", MPV_FORMAT_STRING, &title_cstr) == 0) {
                if (title_cstr) {
                    m_active_station->setCurrentTitle(title_cstr);
                    mpv_free(title_cstr);
                }
            }
            int buffering_flag = 0;
             if (mpv_get_property(m_active_station->getMpvHandle(), "core-idle", MPV_FORMAT_FLAG, &buffering_flag) == 0) {
                 m_active_station->setBuffering(buffering_flag);
             }
        }

        std::string status_string = "Connecting...";
        if (m_active_station) {
            if (m_active_station->isBuffering()) {
                status_string = "Buffering...";
            } else {
                status_string = m_active_station->getCurrentTitle();
            }
            if (m_active_station->getPlaybackState() == PlaybackState::Muted) {
                status_string = "🔇 Muted";
            }
        }

        const std::string& current_station_name =
            (m_current_index < (int)m_candidates.size()) ? m_candidates[m_current_index].first : "Finished!";

        m_ui->draw(m_genre, m_current_index, m_candidates.size(), m_kept_stations.size(),
                   current_station_name, status_string);

        int ch = getch();
        if (ch != ERR) {
            handle_input(ch);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    save_curated_list();
}

void CuratorApp::save_curated_list() const {
    std::string filename = m_genre + ".jsonc";
    json stations_json = json::array();
    for (const auto& station_data : m_kept_stations) {
        json station_obj;
        station_obj["name"] = station_data.first;
        station_obj["urls"] = station_data.second;
        stations_json.push_back(station_obj);
    }

    std::ofstream o(filename);
    o << std::setw(4) << stations_json << std::endl;
}
