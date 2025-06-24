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

CuratorApp::CuratorApp(const std::string& genre, std::vector<CuratorStation> candidates)
    : m_genre(genre), m_candidates(std::move(candidates)), m_current_index(0), m_quit_flag(false) {
    m_ui = std::make_unique<CuratorUI>();
    if (!m_candidates.empty()) {
        update_preloaded_stations();
    } else {
        m_quit_flag = true;
    }
}

CuratorApp::~CuratorApp() { m_station_pool.clear(); }

void CuratorApp::update_preloaded_stations() {
    m_station_pool.erase(std::remove_if(m_station_pool.begin(), m_station_pool.end(),
                                        [this](const auto& s) { return s->getID() < m_current_index; }),
                         m_station_pool.end());

    for (int i = 0; i <= PRELOAD_COUNT; ++i) {
        int target_index = m_current_index + i;
        if (target_index >= (int) m_candidates.size())
            break;

        auto it = std::find_if(m_station_pool.begin(), m_station_pool.end(),
                               [target_index](const auto& s) { return s->getID() == target_index; });

        if (it == m_station_pool.end()) {
            const auto& candidate = m_candidates[target_index];
            auto new_station = std::make_unique<RadioStream>(target_index, candidate.name, candidate.urls);
            new_station->initialize(0.0);
            new_station->setPlaybackState(PlaybackState::Muted);
            m_station_pool.push_back(std::move(new_station));
        }
    }

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
    if (m_current_index < (int) m_candidates.size() && keep_current) {
        m_kept_stations.push_back(m_candidates[m_current_index]);
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
    if (it == m_station_pool.end())
        return;
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
                // Poll for the live bitrate from libmpv
                int64_t bitrate_bps = 0;
                if (mpv_get_property(station->getMpvHandle(), "audio-bitrate", MPV_FORMAT_INT64, &bitrate_bps) == 0) {
                    if (bitrate_bps > 0) {
                        station->setBitrate(static_cast<int>(bitrate_bps / 1000));
                    }
                }
            }
        }

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

        // Create a mutable copy of the candidate data to display
        CuratorStation station_to_display =
            (m_current_index < (int) m_candidates.size()) ? m_candidates[m_current_index] : CuratorStation{};

        // If the live station has a valid bitrate, override the stale API data
        if (active_it != m_station_pool.end()) {
            const auto& active_station = *active_it;
            if (active_station->getBitrate() > 0) {
                station_to_display.bitrate = active_station->getBitrate();
            }
        }

        m_ui->draw(m_genre, m_current_index, m_candidates.size(), m_kept_stations.size(), station_to_display,
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
    // Only save the list if the user has kept at least one station.
    if (m_kept_stations.empty()) {
        return;
    }

    std::string filename = m_genre + ".jsonc";
    json stations_json = json::array();
    for (const auto& station_data : m_kept_stations) {
        // We only save the name and urls, as that's what the main player expects.
        json station_obj;
        station_obj["name"] = station_data.name;
        station_obj["urls"] = station_data.urls;
        stations_json.push_back(station_obj);
    }

    std::ofstream o(filename);
    if (o.is_open()) {
        o << std::setw(4) << stations_json << std::endl;
    }
}
