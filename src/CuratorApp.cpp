#include "CuratorApp.h"

#include <mpv/client.h>
#include <ncurses.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

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
        if (target_index >= static_cast<int>(m_candidates.size()))
            break;

        auto it = std::find_if(m_station_pool.begin(), m_station_pool.end(),
                               [target_index](const auto& s) { return s->getID() == target_index; });

        if (it == m_station_pool.end()) {
            const auto& candidate = m_candidates[target_index];
            auto new_station = std::make_unique<RadioStream>(target_index, candidate.name, candidate.urls);
            new_station->initialize(0.0); // Start muted
            new_station->setPlaybackState(PlaybackState::Muted);
            m_station_pool.push_back(std::move(new_station));
        }
    }

    auto active_it = std::find_if(m_station_pool.begin(), m_station_pool.end(),
                                  [this](const auto& s) { return s->getID() == m_current_index; });
    if (active_it != m_station_pool.end()) {
        if ((*active_it)->getPlaybackState() == PlaybackState::Muted && m_is_active_station_playing) {
            (*active_it)->setPlaybackState(PlaybackState::Playing);
            double vol = 100.0;
            mpv_set_property((*active_it)->getMpvHandle(), "volume", MPV_FORMAT_DOUBLE, &vol);
        } else if ((*active_it)->getPlaybackState() == PlaybackState::Playing && !m_is_active_station_playing) {
            (*active_it)->setPlaybackState(PlaybackState::Muted);
            double vol = 0.0;
            mpv_set_property((*active_it)->getMpvHandle(), "volume", MPV_FORMAT_DOUBLE, &vol);
        }
    }
}

void CuratorApp::advance(bool keep_current) {
    m_history.push_back(m_current_index);

    if (m_current_index < static_cast<int>(m_candidates.size()) && keep_current) {
        m_kept_stations.push_back(m_candidates[m_current_index]);
    } else {
        m_discarded_count++;
    }

    m_current_index++;
    m_is_active_station_playing = true;

    if (m_current_index >= static_cast<int>(m_candidates.size())) {
        m_quit_flag = true;
        return;
    }
    update_preloaded_stations();
}

void CuratorApp::go_back() {
    if (m_history.empty())
        return;

    m_current_index = m_history.back();
    m_history.pop_back();

    if (!m_kept_stations.empty() && m_kept_stations.back().name == m_candidates[m_current_index].name) {
        m_kept_stations.pop_back();
    } else {
        if (m_discarded_count > 0)
            m_discarded_count--;
    }
    m_is_active_station_playing = true;
    update_preloaded_stations();
}

// Renamed from edit_tags to handle_edit_tags_action for consistency
void CuratorApp::handle_edit_tags_action() {
    if (m_current_index >= static_cast<int>(m_candidates.size()))
        return;

    // Store original terminal settings
    bool was_echo = isendwin() ? false : true; // Check if ncurses is active
    if (was_echo)
        echo();
    else
        nocbreak(); // if not active (e.g. tests), don't call ncurses functions

    curs_set(1);

    int y = LINES - 2;
    move(y, 0);
    clrtoeol();

    mvprintw(y, 5, "Edit tags (comma separated): ");
    refresh();

    char input[256];
    getnstr(input, sizeof(input) - 1);

    if (was_echo)
        noecho();
    else
        cbreak();
    curs_set(0);

    std::string new_tags_str(input);
    if (!new_tags_str.empty()) {
        std::istringstream ss(new_tags_str);
        std::string tag;
        m_candidates[m_current_index].tags.clear();

        while (std::getline(ss, tag, ',')) {
            tag.erase(0, tag.find_first_not_of(" \t"));
            tag.erase(tag.find_last_not_of(" \t") + 1);
            if (!tag.empty()) {
                m_candidates[m_current_index].tags.push_back(tag);
            }
        }
    }
}

// --- New Input Handler Action Methods ---
void CuratorApp::handle_quit_action() { m_quit_flag = true; }

void CuratorApp::handle_keep_action() { advance(true); }

void CuratorApp::handle_discard_action() { advance(false); }

void CuratorApp::handle_back_action() { go_back(); }

void CuratorApp::handle_play_toggle_action() {
    auto active_station_iter = std::find_if(m_station_pool.begin(), m_station_pool.end(),
                                            [this](const auto& s) { return s->getID() == m_current_index; });

    if (active_station_iter == m_station_pool.end() || !(*active_station_iter) ||
        !(*active_station_iter)->isInitialized()) {
        return;
    }
    RadioStream* active_station_ptr = (*active_station_iter).get();

    if (active_station_ptr->getPlaybackState() == PlaybackState::Muted) {
        active_station_ptr->setPlaybackState(PlaybackState::Playing);
        m_is_active_station_playing = true;
        double vol = 100.0;
        mpv_set_property(active_station_ptr->getMpvHandle(), "volume", MPV_FORMAT_DOUBLE, &vol);
    } else {
        active_station_ptr->setPlaybackState(PlaybackState::Muted);
        m_is_active_station_playing = false;
        double vol = 0.0;
        mpv_set_property(active_station_ptr->getMpvHandle(), "volume", MPV_FORMAT_DOUBLE, &vol);
    }
}

// --- Main Input Dispatcher ---
void CuratorApp::handle_input(int ch) {
    switch (ch) {
    case 'q':
    case 'Q':
        handle_quit_action();
        break;
    case 'k':
    case 'K':
        handle_keep_action();
        break;
    case 'd':
    case 'D':
        handle_discard_action();
        break;
    case 'b':
    case 'B':
        handle_back_action();
        break;
    case 'e':
    case 'E':
        handle_edit_tags_action();
        break;
    case 'p':
    case 'P':
        handle_play_toggle_action();
        break;
    }
}

void CuratorApp::process_mpv_events_for_pool() {
    for (const auto& station_ptr : m_station_pool) {
        if (station_ptr && station_ptr->isInitialized()) {
            char* title_cstr = nullptr;
            if (mpv_get_property(station_ptr->getMpvHandle(), "media-title", MPV_FORMAT_STRING, &title_cstr) == 0) {
                if (title_cstr) {
                    station_ptr->setCurrentTitle(title_cstr);
                    mpv_free(title_cstr);
                }
            }
            int buffering_flag = 0;
            if (mpv_get_property(station_ptr->getMpvHandle(), "core-idle", MPV_FORMAT_FLAG, &buffering_flag) == 0) {
                station_ptr->setBuffering(buffering_flag);
            }
            int64_t bitrate_bps = 0;
            if (mpv_get_property(station_ptr->getMpvHandle(), "audio-bitrate", MPV_FORMAT_INT64, &bitrate_bps) == 0) {
                if (bitrate_bps > 0) {
                    station_ptr->setBitrate(static_cast<int>(bitrate_bps / 1000));
                }
            }

            if (station_ptr->getID() == m_current_index) {
                m_is_active_station_playing = (station_ptr->getPlaybackState() == PlaybackState::Playing);
            }
        }
    }
}

std::string CuratorApp::get_active_station_status_string() const {
    auto active_it = std::find_if(m_station_pool.begin(), m_station_pool.end(),
                                  [this](const auto& s) { return s && s->getID() == m_current_index; });

    if (active_it != m_station_pool.end()) {
        const auto& active_station = *active_it;
        if (!active_station->isInitialized()) {
            return "Initializing...";
        }
        if (active_station->isBuffering()) {
            return "Buffering...";
        }
        if (active_station->getPlaybackState() == PlaybackState::Muted) {
            return "Muted";
        }
        return active_station->getCurrentTitle();
    }
    return "Connecting...";
}

CuratorStation CuratorApp::get_station_display_data() const {
    if (m_current_index < 0 || m_current_index >= static_cast<int>(m_candidates.size())) {
        return CuratorStation{};
    }

    CuratorStation station_to_display = m_candidates[m_current_index];

    auto active_it = std::find_if(m_station_pool.begin(), m_station_pool.end(),
                                  [this](const auto& s) { return s && s->getID() == m_current_index; });

    if (active_it != m_station_pool.end()) {
        const auto& live_station_data = *active_it;
        if (live_station_data->isInitialized() && live_station_data->getBitrate() > 0) {
            station_to_display.bitrate = live_station_data->getBitrate();
        }
    }
    return station_to_display;
}

void CuratorApp::run() {
    while (!m_quit_flag) {
        process_mpv_events_for_pool();

        std::string status_string = get_active_station_status_string();
        CuratorStation station_to_display = get_station_display_data();

        m_ui->draw(m_genre, m_current_index, m_candidates.size(), m_kept_stations.size(), m_discarded_count,
                   station_to_display, status_string, m_is_active_station_playing);

        int ch = getch();
        if (ch != ERR) {
            handle_input(ch);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    save_curated_list();
}

void CuratorApp::save_curated_list() const {
    if (m_kept_stations.empty()) {
        return;
    }

    std::string filename = m_genre + ".jsonc";
    json stations_json_array = json::array();
    for (const auto& station_data : m_kept_stations) {
        json station_obj;
        station_obj["name"] = station_data.name;
        station_obj["urls"] = station_data.urls;

        if (!station_data.tags.empty()) {
            station_obj["tags"] = station_data.tags;
        }
        stations_json_array.push_back(station_obj);
    }

    std::ofstream o(filename);
    if (o.is_open()) {
        o << std::setw(4) << stations_json_array << std::endl;
    }
}
