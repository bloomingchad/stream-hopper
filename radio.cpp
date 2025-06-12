//Radio Switcher - Discovery mode with Fading, Mute, Refactored For Maintainability + Small Mode
//
// sudo dnf install gcc-c++ mpv-devel ncurses-devel pkg-config make
// Download 'json.hpp' from https://github.com/nlohmann/json.
// g++ radio.cpp -o radio $(pkg-config --cflags --libs mpv ncurses) -pthread
//
// - with Audio Fading
// - Mute functionality
// - Restructured
// - JSON Station Name Logging
// - Small Mode: Auto-rotation through all stations
// Thanks to gemini-2.5-pro-preview-06-05 and claude-sonnet-4-20250514

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <algorithm>
#include <mutex>      // ADDED: For protecting the string
#include <cstring>    // ADDED: For strcmp
#include <fstream>    // ADDED: For file I/O
#include <iomanip>    // ADDED: For pretty JSON output
#include <ctime>      // ADDED: For timestamps

#include <mpv/client.h>
#include <ncurses.h>

#include "json.hpp"

// === Configuration ===
#define FADE_TIME_MS 900
#define SMALL_MODE_TOTAL_TIME_SECONDS 720  // 12 minutes total

// === Helper Functions ===
void check_mpv_error(int status, const std::string& context) {
    if (status < 0) {
        if (stdscr != NULL && !isendwin()) {
            endwin();
        }
        std::cerr << "MPV Error (" << context << "): " << mpv_error_string(status) << std::endl;
        exit(1);
    }
}

//================================================================================
// 1. RadioStream Class
//================================================================================
class RadioStream {
public:
    RadioStream(int id, std::string name, std::string url)
        : m_id(id), m_name(std::move(name)), m_url(std::move(url)),
          m_mpv(nullptr), m_current_title("Initializing..."),
          m_is_fading(false), m_target_volume(0.0), m_current_volume(0.0),
          m_is_muted(false), m_pre_mute_volume(100.0) {}

    ~RadioStream() {
        destroy();
    }

    // Disable copying.
    RadioStream(const RadioStream&) = delete;
    RadioStream& operator=(const RadioStream&) = delete;

    // ADDED: Custom move constructor because std::atomic members are not movable.
    RadioStream(RadioStream&& other) noexcept
        : m_id(other.m_id),
          m_name(std::move(other.m_name)),
          m_url(std::move(other.m_url)),
          m_mpv(other.m_mpv)
    {
        // For atomic members, we load the value from the source and store it in the new object.
        m_is_fading.store(other.m_is_fading.load());
        m_target_volume.store(other.m_target_volume.load());
        m_current_volume.store(other.m_current_volume.load());
        m_is_muted.store(other.m_is_muted.load());
        m_pre_mute_volume.store(other.m_pre_mute_volume.load());
        
        // For the string, we must lock its mutex to safely get the value.
        m_current_title = other.getCurrentTitle();

        // IMPORTANT: Nullify the source's mpv handle to prevent double-free.
        other.m_mpv = nullptr;
    }

    // ADDED: Custom move assignment operator.
    RadioStream& operator=(RadioStream&& other) noexcept {
        if (this != &other) {
            destroy(); // Clean up our own resources first.

            m_id = other.m_id;
            m_name = std::move(other.m_name);
            m_url = std::move(other.m_url);
            m_mpv = other.m_mpv;
            
            m_is_fading.store(other.m_is_fading.load());
            m_target_volume.store(other.m_target_volume.load());
            m_current_volume.store(other.m_current_volume.load());
            m_is_muted.store(other.m_is_muted.load());
            m_pre_mute_volume.store(other.m_pre_mute_volume.load());
            m_current_title = other.getCurrentTitle();

            other.m_mpv = nullptr;
        }
        return *this;
    }

    void initialize(double initial_volume) {
        m_mpv = mpv_create();
        if (!m_mpv) {
            throw std::runtime_error("Failed to create MPV instance for " + m_name);
        }
        check_mpv_error(mpv_initialize(m_mpv), "mpv_initialize for " + m_name);
        mpv_set_property_string(m_mpv, "vo", "null");
        mpv_set_property_string(m_mpv, "audio-display", "no");
        mpv_set_property_string(m_mpv, "input-default-bindings", "no");
        mpv_set_property_string(m_mpv, "terminal", "no");
        mpv_set_property_string(m_mpv, "msg-level", "all=warn");
        check_mpv_error(mpv_observe_property(m_mpv, m_id, "media-title", MPV_FORMAT_STRING), "observe media-title");
        check_mpv_error(mpv_observe_property(m_mpv, m_id, "eof-reached", MPV_FORMAT_FLAG), "observe eof-reached");
        const char* cmd[] = {"loadfile", m_url.c_str(), "replace", nullptr};
        check_mpv_error(mpv_command_async(m_mpv, 0, cmd), "loadfile for " + m_name);
        m_current_volume = initial_volume;
        m_target_volume = initial_volume;
        mpv_set_property_async(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE, &initial_volume);
    }

    void destroy() {
        if (m_mpv) {
            mpv_terminate_destroy(m_mpv);
            m_mpv = nullptr;
        }
    }

    std::string getStatusString(bool is_active, bool is_small_mode = false) const {
        if (m_is_muted) return "Muted";
        if (m_is_fading) {
            return m_target_volume > m_current_volume ? "Fading In" : "Fading Out";
        }
        if (is_small_mode && is_active) return "Auto-Playing";
        return is_active ? "Playing" : "Silent";
    }

    int getID() const { return m_id; }
    const std::string& getName() const { return m_name; }
    const std::string& getURL() const { return m_url; }
    mpv_handle* getMpvHandle() const { return m_mpv; }
    
    // CHANGED: Use a mutex to protect the string for thread-safe access.
    std::string getCurrentTitle() const {
        std::lock_guard<std::mutex> lock(m_title_mutex);
        return m_current_title;
    }
    void setCurrentTitle(const std::string& title) {
        std::lock_guard<std::mutex> lock(m_title_mutex);
        m_current_title = title;
    }

    bool isMuted() const { return m_is_muted; }
    void setMuted(bool muted) { m_is_muted = muted; }
    double getCurrentVolume() const { return m_current_volume; }
    void setCurrentVolume(double vol) { m_current_volume = vol; }
    double getPreMuteVolume() const { return m_pre_mute_volume; }
    void setPreMuteVolume(double vol) { m_pre_mute_volume = vol; }
    bool isFading() const { return m_is_fading; }
    void setFading(bool fading) { m_is_fading = fading; }
    double getTargetVolume() const { return m_target_volume; }
    void setTargetVolume(double vol) { m_target_volume = vol; }

private:
    int m_id;
    std::string m_name;
    std::string m_url;
    mpv_handle* m_mpv;

    // CHANGED: m_current_title is now a regular string.
    std::string m_current_title;
    // ADDED: A mutex to protect m_current_title. 'mutable' allows it to be locked in const methods.
    mutable std::mutex m_title_mutex;

    // These can remain atomic as they are trivially copyable.
    std::atomic<bool> m_is_fading;
    std::atomic<double> m_target_volume;
    std::atomic<double> m_current_volume;
    std::atomic<bool> m_is_muted;
    std::atomic<double> m_pre_mute_volume;
};


//================================================================================
// 2. UIManager Class
//================================================================================
class UIManager {
public:
    UIManager() {
        initscr(); cbreak(); noecho(); curs_set(0); keypad(stdscr, TRUE); timeout(100);
    }
    ~UIManager() {
        if (stdscr != NULL && !isendwin()) { endwin(); }
    }
    void draw(const std::vector<RadioStream>& stations, int active_station_idx, bool small_mode_active = false, int remaining_seconds = 0) {
        clear();
        if (small_mode_active) {
            mvprintw(0, 0, "Radio Switcher - SMALL MODE ACTIVE | S: Exit Small Mode | Q: Quit");
            mvprintw(1, 0, "Auto-rotating through all stations. Time left for current: %d seconds", remaining_seconds);
        } else {
            mvprintw(0, 0, "Radio Switcher (Refactored) | Q: Quit | Enter: Mute/Unmute | S: Small Mode");
        }
        
        for (size_t i = 0; i < stations.size(); ++i) {
            const auto& station = stations[i];
            bool is_active = (static_cast<int>(i) == active_station_idx);
            if (is_active) attron(A_REVERSE);
            std::string status = station.getStatusString(is_active, small_mode_active);
            mvprintw(2 + i, 2, "[%s] %s: %s (Vol: %.0f)",
                     status.c_str(), station.getName().c_str(),
                     station.getCurrentTitle().c_str(), station.getCurrentVolume());
            if (is_active) attroff(A_REVERSE);
        }
        
        if (!stations.empty() && !small_mode_active) {
            mvprintw(LINES - 2, 0, "Use UP/DOWN arrows to switch stations.");
        } else if (small_mode_active) {
            mvprintw(LINES - 2, 0, "Small Mode: Discovering radio stations automatically...");
        }
        refresh();
    }
    int getInput() { return getch(); }
};


//================================================================================
// 3. RadioPlayer Class
//================================================================================
class RadioPlayer {
public:
    RadioPlayer(std::vector<std::pair<std::string, std::string>> station_data)
        : m_ui(std::make_unique<UIManager>()), m_active_station_idx(0), m_quit_flag(false),
          m_small_mode_active(false), m_small_mode_start_time(std::chrono::steady_clock::now()),
          m_station_switch_duration(0) {
        if (station_data.empty()) {
            throw std::runtime_error("No radio stations provided.");
        }
        for (size_t i = 0; i < station_data.size(); ++i) {
            m_stations.emplace_back(i, station_data[i].first, station_data[i].second);
        }
        
        // Calculate time per station for small mode
        m_station_switch_duration = SMALL_MODE_TOTAL_TIME_SECONDS / static_cast<int>(m_stations.size());

        // ADDED: Load previous song history and initialize for current stations
        load_history_from_disk();
        
        for (size_t i = 0; i < m_stations.size(); ++i) {
            double initial_volume = (i == m_active_station_idx) ? 100.0 : 0.0;
            m_stations[i].initialize(initial_volume);
        }
    }
    
    ~RadioPlayer() {
        m_quit_flag = true;
        if (m_mpv_event_thread.joinable()) {
            m_mpv_event_thread.join();
        }
        // ADDED: Save final history on exit
        save_history_to_disk();
    }
    
    void run() {
        m_mpv_event_thread = std::thread(&RadioPlayer::mpv_event_loop, this);
        bool needs_redraw = true;
        
        while (!m_quit_flag) {
            if (needs_redraw) {
                int remaining_seconds = 0;
                if (m_small_mode_active) {
                    remaining_seconds = get_remaining_seconds_for_current_station();
                }
                m_ui->draw(m_stations, m_active_station_idx, m_small_mode_active, remaining_seconds);
                needs_redraw = false;
            }
            
            // Handle small mode auto-switching
            if (m_small_mode_active && should_switch_station()) {
                int next_station = (m_active_station_idx + 1) % static_cast<int>(m_stations.size());
                switch_station(next_station);
                m_small_mode_start_time = std::chrono::steady_clock::now();
                needs_redraw = true;
            }
            
            int ch = m_ui->getInput();
            if (ch != ERR) {
                handle_input(ch);
                needs_redraw = true;
            }
            
            for (const auto& station : m_stations) {
                if (station.isFading()) {
                    needs_redraw = true;
                    break;
                }
            }
            
            // Update display more frequently in small mode to show countdown
            if (m_small_mode_active) {
                needs_redraw = true;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

private:
    std::vector<RadioStream> m_stations;
    std::unique_ptr<UIManager> m_ui;
    int m_active_station_idx;
    std::atomic<bool> m_quit_flag;
    std::thread m_mpv_event_thread;
    
    // Small mode variables
    std::atomic<bool> m_small_mode_active;
    std::chrono::steady_clock::time_point m_small_mode_start_time;
    int m_station_switch_duration; // seconds per station

    // ADDED: For JSON song history
    nlohmann::json m_song_history;
    std::mutex m_history_mutex;

    void handle_input(int ch) {
        switch (ch) {
            case KEY_UP:
                if (!m_small_mode_active && m_active_station_idx > 0) { 
                    switch_station(m_active_station_idx - 1); 
                }
                break;
            case KEY_DOWN:
                if (!m_small_mode_active && m_active_station_idx < static_cast<int>(m_stations.size()) - 1) { 
                    switch_station(m_active_station_idx + 1); 
                }
                break;
            case '\n': case '\r': case KEY_ENTER:
                if (!m_small_mode_active) {
                    toggle_mute_station(m_active_station_idx);
                }
                break;
            case 's': case 'S':
                toggle_small_mode();
                break;
            case 'q': case 'Q':
                m_quit_flag = true;
                break;
        }
    }

    void toggle_small_mode() {
        if (m_small_mode_active) {
            // Exit small mode
            m_small_mode_active = false;
        } else {
            // Enter small mode
            m_small_mode_active = true;
            m_small_mode_start_time = std::chrono::steady_clock::now();
            
            // Ensure current station is playing and not muted
            RadioStream& current_station = m_stations[m_active_station_idx];
            if (current_station.isMuted()) {
                current_station.setMuted(false);
                fade_audio(current_station, current_station.getCurrentVolume(), current_station.getPreMuteVolume(), FADE_TIME_MS / 2);
            } else if (current_station.getCurrentVolume() < 50.0) {
                fade_audio(current_station, current_station.getCurrentVolume(), 100.0, FADE_TIME_MS);
            }
        }
    }

    bool should_switch_station() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_small_mode_start_time);
        return elapsed.count() >= m_station_switch_duration;
    }

    int get_remaining_seconds_for_current_station() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_small_mode_start_time);
        return std::max(0, m_station_switch_duration - static_cast<int>(elapsed.count()));
    }

    void switch_station(int new_idx) {
        if (new_idx == m_active_station_idx) return;
        
        RadioStream& current_station = m_stations[m_active_station_idx];
        if (!current_station.isMuted()) {
            fade_audio(current_station, current_station.getCurrentVolume(), 0.0, FADE_TIME_MS);
        }
        
        RadioStream& new_station = m_stations[new_idx];
        if (!new_station.isMuted()) {
            fade_audio(new_station, new_station.getCurrentVolume(), 100.0, FADE_TIME_MS);
        }
        
        m_active_station_idx = new_idx;
    }

    void toggle_mute_station(int station_idx) {
        RadioStream& station = m_stations[station_idx];
        if (station.isMuted()) {
            station.setMuted(false);
            fade_audio(station, station.getCurrentVolume(), station.getPreMuteVolume(), FADE_TIME_MS / 2);
        } else {
            station.setPreMuteVolume(station.getCurrentVolume());
            station.setMuted(true);
            fade_audio(station, station.getCurrentVolume(), 0.0, FADE_TIME_MS / 2);
        }
    }

    void fade_audio(RadioStream& station, double from_vol, double to_vol, int duration_ms) {
        station.setFading(true);
        station.setTargetVolume(to_vol);
        std::thread([this, &station, from_vol, to_vol, duration_ms]() {
            const int steps = 50;
            const int step_delay = duration_ms / steps;
            const double vol_step = (to_vol - from_vol) / steps;
            double current_vol = from_vol;
            for (int i = 0; i < steps && !m_quit_flag; ++i) {
                current_vol += vol_step;
                station.setCurrentVolume(current_vol);
                double clamped_vol = std::max(0.0, std::min(100.0, current_vol));
                mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &clamped_vol);
                std::this_thread::sleep_for(std::chrono::milliseconds(step_delay));
            }
            if (!m_quit_flag) {
                station.setCurrentVolume(to_vol);
                double final_vol = std::max(0.0, std::min(100.0, to_vol));
                mpv_set_property_async(station.getMpvHandle(), 0, "volume", MPV_FORMAT_DOUBLE, &final_vol);
            }
            station.setFading(false);
        }).detach();
    }

    void mpv_event_loop() {
        while (!m_quit_flag) {
            bool event_found = false;
            for (auto& station : m_stations) {
                if (!station.getMpvHandle()) continue;
                mpv_event* event = mpv_wait_event(station.getMpvHandle(), 0.001);
                if (event->event_id != MPV_EVENT_NONE) {
                    handle_mpv_event(event);
                    event_found = true;
                }
            }
            if (!event_found) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }

    void handle_mpv_event(mpv_event* event) {
        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            mpv_event_property* prop = (mpv_event_property*)event->data;
            int station_id = event->reply_userdata;
            auto it = std::find_if(m_stations.begin(), m_stations.end(), 
                                   [station_id](const RadioStream& s) { return s.getID() == station_id; });
            if (it != m_stations.end()) {
                RadioStream& station = *it;
                if (strcmp(prop->name, "media-title") == 0 && prop->format == MPV_FORMAT_STRING) {
                    char* title_cstr = *(char**)prop->data;
                    std::string new_title = title_cstr ? title_cstr : "N/A";

                    // Only log if the title has actually changed to something meaningful.
                    if (new_title != station.getCurrentTitle()) {
                        if (new_title != "N/A" && new_title != "Initializing...") {
                            // Get current time as a string
                            auto now = std::chrono::system_clock::now();
                            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
                            char time_buf[100];
                            std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_c));

                            // Create JSON entry: ["time", "title"]
                            nlohmann::json song_entry = {std::string(time_buf), new_title};
                            
                            { // Lock before modifying the shared history object
                                std::lock_guard<std::mutex> lock(m_history_mutex);
                                m_song_history[station.getName()].push_back(song_entry);
                            }
                            // Flush the updated history to disk immediately
                            save_history_to_disk();
                        }
                        // Always update the station's current title for the UI
                        station.setCurrentTitle(new_title);
                    }
                } else if (strcmp(prop->name, "eof-reached") == 0 && prop->format == MPV_FORMAT_FLAG) {
                    if (*(int*)prop->data) {
                        station.setCurrentTitle("Stream Error - Reconnecting...");
                        const char* cmd[] = {"loadfile", station.getURL().c_str(), "replace", nullptr};
                        mpv_command_async(station.getMpvHandle(), 0, cmd);
                    }
                }
            }
        }
    }
    
    // ADDED: Methods to load and save the JSON song history
    void load_history_from_disk() {
        std::ifstream i("radio_history.json");
        if (i.is_open()) {
            try {
                i >> m_song_history;
                if (!m_song_history.is_object()) {
                    m_song_history = nlohmann::json::object(); // Ensure it's a JSON object
                }
            } catch (...) {
                // If file is corrupted or not valid JSON, start fresh
                m_song_history = nlohmann::json::object();
            }
        }
        // Ensure all currently configured stations have an entry in the history
        for (const auto& station : m_stations) {
            if (!m_song_history.contains(station.getName())) {
                m_song_history[station.getName()] = nlohmann::json::array();
            }
        }
    }

    void save_history_to_disk() {
        std::lock_guard<std::mutex> lock(m_history_mutex);
        std::ofstream o("radio_history.json");
        if (o.is_open()) {
            // Write the JSON object to the file with pretty printing
            o << std::setw(4) << m_song_history << std::endl;
        }
    }
};


//================================================================================
// 4. Main Function
//================================================================================
int main() {
    std::vector<std::pair<std::string, std::string>> station_data = {
        //{"ILoveRadio", "https://ilm.stream18.radiohost.de/ilm_iloveradio_mp3-192"},
        {"ILove2Dance", "https://ilm.stream18.radiohost.de/ilm_ilove2dance_mp3-192"},

        {"RM Deutschrap", "https://rautemusik.stream43.radiohost.de/rm-deutschrap-charts_mp3-192"},
        {"RM Charthits", "https://rautemusik.stream43.radiohost.de/charthits"},

        {"bigFM OldSchool Deutschrap", "https://stream.bigfm.de/oldschooldeutschrap/aac-128"},
        {"bigFM Dance", "https://audiotainment-sw.streamabc.net/atsw-dance-aac-128-2321625"},
        //{"bigFM DanceHall", "https://audiotainment-sw.streamabc.net/atsw-dancehall-aac-128-5420319"},
        //{"bigFM GrooveNight", "https://audiotainment-sw.streamabc.net/atsw-groovenight-aac-128-5346495"},

        {"BreakZ", "https://rautemusik.stream43.radiohost.de/breakz"},

        //{"1.fm DeepHouse", "http://strm112.1.fm/deephouse_mobile_mp3"},
        {"1.fm Dance", "https://strm112.1.fm/dance_mobile_mp3"},

        {"104.6rtl Dance Hits", "https://stream.104.6rtl.com/dance-hits/mp3-128/konsole"},
        //{"Absolut.de Hot", "https://edge22.live-sm.absolutradio.de/absolut-hot"},

        {"Sunshine Live - EDM", "http://stream.sunshine-live.de/edm/mp3-192/stream.sunshine-live.de/"},
        {"Sunshine Live - Amsterdam Club", "https://stream.sunshine-live.de/ade18club/mp3-128"},
        {"Sunshine Live - Charts", "http://stream.sunshine-live.de/sp6/mp3-128"},
        //{"Sunshine Live - EuroDance", "https://sunsl.streamabc.net/sunsl-eurodance-mp3-192-9832422"},
        //{"Sunshine Live - Summer Beats", "http://stream.sunshine-live.de/sp2/aac-64"},

        {"Kiss FM - German Beats", "https://topradio-stream31.radiohost.de/kissfm-deutschrap-hits_mp3-128"},
        //{"Kiss FM - DJ Sets", "https://topradio.stream41.radiohost.de/kissfm-electro_mp3-192"},
        {"Kiss FM - Remix", "https://topradio.stream05.radiohost.de/kissfm-remix_mp3-192"},
        //{"Kiss FM - Events", "https://topradio.stream10.radiohost.de/kissfm-event_mp3-192"},

        {"Energy - Dance", "https://edge01.streamonkey.net/energy-dance/stream/mp3"},
        //{"Energy - MasterMix", "https://scdn.nrjaudio.fm/adwz1/de/33027/mp3_128.mp3"},
        {"Energy - Deutschrap", "https://edge07.streamonkey.net/energy-deutschrap"},

        {"PulseEDM Dance Radio", "http://pulseedm.cdnstream1.com:8124/1373_128.m3u"},
        {"Puls Radio Dance", "https://sc4.gergosnet.com/pulsHD.mp3"},
        {"Puls Radio Club", "https://str3.openstream.co/2138"},
        {"Los 40 Dance", "https://playerservices.streamtheworld.com/api/livestream-redirect/LOS40_DANCEAAC.aac"},
        {"RadCap Uplifting", "http://79.111.119.111:8000/upliftingtrance"},
        {"Regenbogen", "https://streams.regenbogen.de/"},
        {"RadCap ClubDance", "http://79.111.119.111:8000/clubdance"},
    };

    try {
        RadioPlayer player(station_data);
        player.run();
    } catch (const std::exception& e) {
        if (stdscr != NULL && !isendwin()) {
            endwin();
        }
        std::cerr << "Critical Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Radio player exited gracefully." << std::endl;
    return 0;
}