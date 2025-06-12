//Radio Switcher - Discovery mode with Fading, Mute, Refactored For Maintainability
//
// sudo dnf install gcc-c++ mpv-devel ncurses-devel pkg-config make
// g++ radio.cpp -o radio $(pkg-config --cflags --libs mpv ncurses) -pthread
//
// - with Audio Fading
// - Mute functionality
// - Restructured
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

#include <mpv/client.h>
#include <ncurses.h>

// === Configuration ===
#define FADE_TIME_MS 900

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

    std::string getStatusString(bool is_active) const {
        if (m_is_muted) return "Muted";
        if (m_is_fading) {
            return m_target_volume > m_current_volume ? "Fading In" : "Fading Out";
        }
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
    void draw(const std::vector<RadioStream>& stations, int active_station_idx) {
        clear();
        mvprintw(0, 0, "Radio Switcher (Refactored) | Q: Quit | Enter: Mute/Unmute");
        for (size_t i = 0; i < stations.size(); ++i) {
            const auto& station = stations[i];
            bool is_active = (static_cast<int>(i) == active_station_idx);
            if (is_active) attron(A_REVERSE);
            std::string status = station.getStatusString(is_active);
            mvprintw(2 + i, 2, "[%s] %s: %s (Vol: %.0f)",
                     status.c_str(), station.getName().c_str(),
                     station.getCurrentTitle().c_str(), station.getCurrentVolume());
            if (is_active) attroff(A_REVERSE);
        }
        if (!stations.empty()) {
            mvprintw(LINES - 2, 0, "Use UP/DOWN arrows to switch stations.");
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
        : m_ui(std::make_unique<UIManager>()), m_active_station_idx(0), m_quit_flag(false) {
        if (station_data.empty()) {
            throw std::runtime_error("No radio stations provided.");
        }
        for (size_t i = 0; i < station_data.size(); ++i) {
            m_stations.emplace_back(i, station_data[i].first, station_data[i].second);
        }
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
    }
    void run() {
        m_mpv_event_thread = std::thread(&RadioPlayer::mpv_event_loop, this);
        bool needs_redraw = true;
        while (!m_quit_flag) {
            if (needs_redraw) {
                m_ui->draw(m_stations, m_active_station_idx);
                needs_redraw = false;
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
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

private:
    std::vector<RadioStream> m_stations;
    std::unique_ptr<UIManager> m_ui;
    int m_active_station_idx;
    std::atomic<bool> m_quit_flag;
    std::thread m_mpv_event_thread;

    void handle_input(int ch) {
        switch (ch) {
            case KEY_UP:
                if (m_active_station_idx > 0) { switch_station(m_active_station_idx - 1); }
                break;
            case KEY_DOWN:
                if (m_active_station_idx < static_cast<int>(m_stations.size()) - 1) { switch_station(m_active_station_idx + 1); }
                break;
            case '\n': case '\r': case KEY_ENTER:
                toggle_mute_station(m_active_station_idx);
                break;
            case 'q': case 'Q':
                m_quit_flag = true;
                break;
        }
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
                    char* title = *(char**)prop->data;
                    station.setCurrentTitle(title ? title : "N/A");
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
};


//================================================================================
// 4. Main Function
//================================================================================
int main() {
    std::vector<std::pair<std::string, std::string>> station_data = {
        {"ILoveRadio", "https://ilm.stream18.radiohost.de/ilm_iloveradio_mp3-192"},
        {"ILove2Dance", "https://ilm.stream18.radiohost.de/ilm_ilove2dance_mp3-192"},

        {"RM Deutschrap", "https://rautemusik.stream43.radiohost.de/rm-deutschrap-charts_mp3-192"},
        {"RM Charthits", "https://rautemusik.stream43.radiohost.de/charthits"},

        {"bigFM OldSchool Deutschrap", "https://stream.bigfm.de/oldschooldeutschrap/aac-128"},
        {"bigFM Dance", "https://audiotainment-sw.streamabc.net/atsw-dance-aac-128-2321625"},
        {"bigFM DanceHall", "https://audiotainment-sw.streamabc.net/atsw-dancehall-aac-128-5420319"},
        {"bigFM GrooveNight", "https://audiotainment-sw.streamabc.net/atsw-groovenight-aac-128-5346495"},

        {"BreakZ", "https://rautemusik.stream43.radiohost.de/breakz"},

        {"1.fm DeepHouse", "http://strm112.1.fm/deephouse_mobile_mp3"},
        {"1.fm Dance", "https://strm112.1.fm/dance_mobile_mp3"},

        {"104.6rtl Dance Hits", "https://stream.104.6rtl.com/dance-hits/mp3-128/konsole"},
        {"Absolut.de Hot", "https://edge22.live-sm.absolutradio.de/absolut-hot"},

        {"Sunshine Live - EDM", "http://stream.sunshine-live.de/edm/mp3-192/stream.sunshine-live.de/"},
        {"Sunshine Live - Amsterdam Club", "https://stream.sunshine-live.de/ade18club/mp3-128"},
        {"Sunshine Live - Charts", "http://stream.sunshine-live.de/sp6/mp3-128"},
        {"Sunshine Live - EuroDance", "https://sunsl.streamabc.net/sunsl-eurodance-mp3-192-9832422"},
        {"Sunshine Live - Summer Beats", "http://stream.sunshine-live.de/sp2/aac-64"},

        {"Kiss FM - German Beats", "https://topradio-stream31.radiohost.de/kissfm-deutschrap-hits_mp3-128"},
        {"Kiss FM - DJ Sets", "https://topradio.stream41.radiohost.de/kissfm-electro_mp3-192"},
        {"Kiss FM - Remix", "https://topradio.stream05.radiohost.de/kissfm-remix_mp3-192"},
        {"Kiss FM - Events", "https://topradio.stream10.radiohost.de/kissfm-event_mp3-192"},

        {"Energy - Dance", "https://edge01.streamonkey.net/energy-dance/stream/mp3"},
        {"Energy - MasterMix", "https://scdn.nrjaudio.fm/adwz1/de/33027/mp3_128.mp3"},
        {"Energy - Deutschrap", "https://edge07.streamonkey.net/energy-deutschrap"},

        {"PulseEDM Dance Radio", "http://pulseedm.cdnstream1.com:8124/1373_128.m3u"}
    };

    try {
        RadioPlayer player(station_data);
        player.run();
    } catch (const std::exception& e) {
        std::cerr << "Critical Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Radio player exited gracefully." << std::endl;
    return 0;
}