#include "CuratorApp.h"

#include <mpv/client.h>
#include <ncurses.h>

#include <algorithm> // for std::find_if
#include <fstream>
#include <iomanip>
#include <thread> // for sleep_for

#include "CuratorUI.h"
#include "Utils.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

CuratorApp::CuratorApp(const std::string& genre, StationData candidates)
    : m_genre(genre),
      m_candidates(std::move(candidates)),
      m_current_index(0),
      m_quit_flag(false) {
    m_ui = std::make_unique<CuratorUI>();
    if (!m_candidates.empty()) {
        update_preloaded_stations();
    } else {
        m_quit_flag = true; // No candidates, nothing to do.
    }
}

CuratorApp::~CuratorApp() {
    // RadioStream destructors handle their own mpv instance shutdown.
    m_station_pool.clear();
}

void CuratorApp::update_preloaded_stations() {
    // Remove streams that are no longer in the preload window
    m_station_pool.erase(
        std::remove_if(m_station_pool.begin(), m_station_pool.end(),
                       [this](const auto& s) { return s->getID() < m_current_index; }),
        m_station_pool.end());

    // Add new streams that have entered the preload window
    for (int i = 0; i <= PRELOAD_COUNT; ++i) {
        int target_index = m_current_index + i;
        if (target_index >= (int) m_candidates.size())
            break; // Don't preload past the end of the list

        // Check if a stream for this index already exists
        auto it = std::find_if(m_station_pool.begin(), m_station_pool.end(),
                               [target_index](const auto& s) { return s->getID() == target_index; });

        if (it == m_station_pool.end()) {
            // It doesn't exist, so create and preload it
            const auto& candidate = m_candidates[target_index];
            auto new_station = std::make_unique<RadioStream>(target_index, candidate.first, candidate.second);
            // Preloaded stations start muted at 0 volume. The active one will be unmuted.
            new_station->initialize(0.0);
            new_station->setPlaybackState(PlaybackState::Muted);
            m_station_pool.push_back(std::move(new_station));
        }
    }

    // Ensure the current active station is playing at full volume
    auto it = std::find_if(m_station_pool.begin(), m_station_pool.end(),
                           [this](const auto& s) { return s->getID() == m_current_index; });
    if (it != m_station_pool.end()) {
        if ((*it)->getPlaybackState() == PlaybackState::Muted) {
             (*it)->setPlaybackState(PlaybackState::Playing);
             double vol = 100.0;
             mpv_set_property((*it)->getMpvHandle(), "volume", MPV_FORMAT_DOUBLE, &vol);
        }
    }
}

void CuratorApp::advance(bool keep_current) {
    if (m_current_index < (int) m_candidates.size()) {
        if (keep_current) {
            m_kept_stations.push_back(m_candidates[m_current_index]);
        }
    }

    m_current_index++;

    if (m_current_index >= (int) m_candidates.size()) {
        m_quit_flag = true;
        return;
    }
    update_preloaded_stations();
}

void CuratorApp::handle_input(int ch) {
    auto it = std::find_if(m_station_pool.begin(), m_station_pool.end(),
                           [this](const auto& s) { return s->getID() == m_current_index; });
    if (it == m_station_pool.end()) return; // No active station
    auto& active_station = *it;

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
        if (active_station->isInitialized()) {
            if (active_station->getPlaybackState() == PlaybackState::Muted) {
                active_station->setPlaybackState(PlaybackState::Playing);
                double vol = 100.0;
                mpv_set_property(active_station->getMpvHandle(), "volume", MPV_FORMAT_DOUBLE, &vol);
            } else {
                active_station->setPlaybackState(PlaybackState::Muted);
                double vol = 0.0;
                mpv_set_property(active_station->getMpvHandle(), "volume", MPV_FORMAT_DOUBLE, &vol);
            }
        }
        break;
    }
}

void CuratorApp::run() {
    while (!m_quit_flag) {
        // Poll mpv properties for all stations in the pool
        for (const auto& station : m_station_pool) {
            if (station && station->isInitialized()) {
                char* title_cstr = nullptr;
                if (mpv_get_property(station->getMpvHandle(), "media-title", MPV_FORMAT_STRING, &title_cstr) == 0) {
                    if (title_cstr) {
                        station->setCurrentTitle(title_cstr);
                        mpv_free(title_cstr);
                    }
                }
                int buffering_flag = 0;
                if (mpv_get_property(station->getMpvHandle(), "core-idle", MPV_FORMAT_FLAG, &buffering_flag) == 0) {
                    station->setBuffering(buffering_flag);
                }
            }
        }

        // Get display info from the current active station
        std::string status_string = "Connecting...";
        auto active_it = std::find_if(m_station_pool.begin(), m_station_pool.end(),
                                     [this](const auto& s) { return s->getID() == m_current_index; });

        if (active_it != m_station_pool.end()) {
            const auto& active_station = *active_it;
             if (active_station->isBuffering()) {
                status_string = "Buffering...";
            } else {
                status_string = active_station->getCurrentTitle();
            }
            if (active_station->getPlaybackState() == PlaybackState::Muted) {
                status_string = "🔇 Muted";
            }
        }

        const std::string& current_station_name =
            (m_current_index < (int) m_candidates.size()) ? m_candidates[m_current_index].first : "Finished!";

        m_ui->draw(m_genre, m_current_index, m_candidates.size(), m_kept_stations.size(), current_station_name,
                   status_string);

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
