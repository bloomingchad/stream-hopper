//Radio Switcher - Discovery mode
//sudo dnf install gcc-c++ mpv-devel ncurses-devel pkg-config make
//g++ radio.cpp -o radio $(pkg-config --cflags --libs mpv ncurses) -pthread
//
// - with Audio Fading

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib> // For getenv
#include <cstring> // For strcmp
#include <atomic>

#include <mpv/client.h>
#include <ncurses.h>

#define FADE_TIME 900 // Fade duration in milliseconds

// Helper to check for MPV errors
void check_mpv_error(int status, const std::string& context) {
    if (status < 0) {
        bool ncurses_active = (stdscr != NULL && !isendwin());
        if (ncurses_active) {
            mvprintw(LINES - 1, 0, "MPV Error (%s): %s. Exiting.", context.c_str(), mpv_error_string(status));
            refresh();
            std::this_thread::sleep_for(std::chrono::seconds(5));
            endwin();
        } else {
            std::cerr << "MPV Error (" << context << "): " << mpv_error_string(status) << std::endl;
        }
        exit(1);
    }
}

struct RadioStream {
    mpv_handle *mpv;
    std::string url;
    std::string name;
    std::string current_title;
    int id;
    std::atomic<bool> is_fading;
    std::atomic<double> target_volume;
    std::atomic<double> current_volume;

    RadioStream(int unique_id, const std::string& n, const std::string& u)
        : mpv(nullptr), url(u), name(n), current_title("Loading..."), id(unique_id),
          is_fading(false), target_volume(0.0), current_volume(0.0) {}

    // Delete copy constructor and copy assignment
    RadioStream(const RadioStream&) = delete;
    RadioStream& operator=(const RadioStream&) = delete;

    // Custom move constructor
    RadioStream(RadioStream&& other) noexcept
        : mpv(other.mpv), url(std::move(other.url)), name(std::move(other.name)),
          current_title(std::move(other.current_title)), id(other.id),
          is_fading(other.is_fading.load()), target_volume(other.target_volume.load()),
          current_volume(other.current_volume.load()) {
        other.mpv = nullptr;
    }

    // Custom move assignment
    RadioStream& operator=(RadioStream&& other) noexcept {
        if (this != &other) {
            mpv = other.mpv;
            url = std::move(other.url);
            name = std::move(other.name);
            current_title = std::move(other.current_title);
            id = other.id;
            is_fading.store(other.is_fading.load());
            target_volume.store(other.target_volume.load());
            current_volume.store(other.current_volume.load());
            other.mpv = nullptr;
        }
        return *this;
    }
};

std::vector<RadioStream> g_stations;
int g_active_station_idx = 0;
bool g_needs_redraw = true;
volatile bool g_quit_flag = false;

void fade_audio(RadioStream& station, double from_vol, double to_vol, int duration_ms) {
    if (!station.mpv || g_quit_flag) return;
    
    station.is_fading = true;
    station.target_volume = to_vol;
    
    const int steps = 50; // Number of fade steps
    const int step_delay = duration_ms / steps;
    const double vol_step = (to_vol - from_vol) / steps;
    
    std::thread([&station, from_vol, vol_step, step_delay, steps, to_vol]() {
        double current_vol = from_vol;
        
        for (int i = 0; i < steps && !g_quit_flag && station.mpv; ++i) {
            current_vol += vol_step;
            station.current_volume = current_vol;
            
            // Clamp volume between 0 and 100
            double clamped_vol = std::max(0.0, std::min(100.0, current_vol));
            mpv_set_property_async(station.mpv, 0, "volume", MPV_FORMAT_DOUBLE, &clamped_vol);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(step_delay));
        }
        
        // Ensure final volume is set
        if (!g_quit_flag && station.mpv) {
            station.current_volume = to_vol;
            double final_vol = std::max(0.0, std::min(100.0, to_vol));
            mpv_set_property_async(station.mpv, 0, "volume", MPV_FORMAT_DOUBLE, &final_vol);
        }
        
        station.is_fading = false;
    }).detach();
}

void switch_station(int new_station_idx) {
    if (new_station_idx < 0 || new_station_idx >= static_cast<int>(g_stations.size())) {
        return;
    }
    
    if (g_active_station_idx == new_station_idx) {
        return;
    }
    
    // Fade out current station
    if (g_active_station_idx >= 0 && g_active_station_idx < static_cast<int>(g_stations.size())) {
        RadioStream& current_station = g_stations[g_active_station_idx];
        if (current_station.mpv) {
            fade_audio(current_station, current_station.current_volume, 0.0, FADE_TIME);
        }
    }
    
    // Fade in new station
    RadioStream& new_station = g_stations[new_station_idx];
    if (new_station.mpv) {
        fade_audio(new_station, new_station.current_volume, 100.0, FADE_TIME);
    }
    
    g_active_station_idx = new_station_idx;
}

void redraw_ui() {
    if (g_quit_flag) return;
    clear();
    mvprintw(0, 0, "Radio Switcher - Press Q to quit (Fade: %dms)", FADE_TIME);
    for (size_t i = 0; i < g_stations.size(); ++i) {
        std::string status;
        if (static_cast<int>(i) == g_active_station_idx) {
            if (g_stations[i].is_fading) {
                status = g_stations[i].target_volume > 50.0 ? "Fading In" : "Fading Out";
            } else {
                status = "Playing";
            }
            attron(A_REVERSE);
        } else {
            if (g_stations[i].is_fading) {
                status = "Fading Out";
            } else {
                status = "Silent";
            }
        }
        
        mvprintw(2 + i, 2, "[%s] %s: %s (Vol: %.0f)",
                 status.c_str(),
                 g_stations[i].name.c_str(),
                 g_stations[i].current_title.c_str(),
                 g_stations[i].current_volume.load());
        
        if (static_cast<int>(i) == g_active_station_idx) {
            attroff(A_REVERSE);
        }
    }
    if (!g_stations.empty() && g_active_station_idx >= 0 && static_cast<size_t>(g_active_station_idx) < g_stations.size()) {
        mvprintw(LINES - 2, 0, "UP/DOWN to switch. Active: %s", g_stations[g_active_station_idx].name.c_str());
    } else if (!g_stations.empty()) {
         mvprintw(LINES - 2, 0, "UP/DOWN to switch. Initializing...");
    } else {
        mvprintw(LINES - 2, 0, "UP/DOWN to switch. No active station.");
    }
    refresh();
    g_needs_redraw = false;
}

void mpv_event_loop() {
    while (!g_quit_flag) {
        bool event_processed_this_cycle = false;
        for (size_t i = 0; i < g_stations.size(); ++i) {
            if (g_quit_flag) break;
            if (!g_stations[i].mpv) continue;

            mpv_event *event = mpv_wait_event(g_stations[i].mpv, 0.001); // 1ms timeout

            if (event && event->event_id != MPV_EVENT_NONE) {
                event_processed_this_cycle = true;
                if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
                    mpv_event_property *prop = (mpv_event_property *)event->data;
                    int station_id_from_event = event->reply_userdata;

                    for(size_t k=0; k < g_stations.size(); ++k) {
                        if (g_stations[k].id == station_id_from_event) {
                            if (strcmp(prop->name, "media-title") == 0) {
                                if (prop->format == MPV_FORMAT_STRING) {
                                    char *title_ptr = nullptr;
                                    if (prop->data) { // Check if prop->data is not null
                                        title_ptr = *(char **)prop->data;
                                    }
                                    g_stations[k].current_title = title_ptr ? title_ptr : "N/A";
                                    g_needs_redraw = true;
                                }
                            } else if (strcmp(prop->name, "eof-reached") == 0) {
                                // **** THIS IS THE FIX for the SEGFAULT ****
                                if (prop->format == MPV_FORMAT_FLAG && prop->data != nullptr) {
                                    if (*(int*)prop->data) { // Dereference only if data is valid
                                        g_stations[k].current_title = "Stream Ended/Error - Retrying...";
                                        g_needs_redraw = true;
                                        const char *cmd_reload[] = {"loadfile", g_stations[k].url.c_str(), "replace", nullptr};
                                        mpv_command_async(g_stations[k].mpv, 0, cmd_reload);
                                    }
                                }
                                // Optional: log if format is unexpected or data is null
                                /* else {
                                    fprintf(stderr, "Warning: eof-reached event with unexpected format/null data for %s. Format: %d, Data: %p\n", 
                                            g_stations[k].name.c_str(), prop->format, prop->data);
                                } */
                            }
                            break; // Found the station, break inner loop
                        }
                    }
                } else if (event->event_id == MPV_EVENT_SHUTDOWN) {
                    // This mpv instance is shutting down.
                    // Main loop's g_quit_flag will handle full program termination.
                }
            }
        }

        if (!event_processed_this_cycle && !g_quit_flag) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Update UI if any station is fading (to show volume changes)
        bool any_fading = false;
        for (const auto& station : g_stations) {
            if (station.is_fading) {
                any_fading = true;
                break;
            }
        }
        if (any_fading) {
            g_needs_redraw = true;
        }
    }
}


int main(int argc, char *argv[]) {
    g_stations.emplace_back(0, "ILoveRadio", "https://ilm.stream18.radiohost.de/ilm_iloveradio_mp3-192");
    g_stations.emplace_back(1, "ILove2Dance", "https://ilm.stream18.radiohost.de/ilm_ilove2dance_mp3-192");
    g_stations.emplace_back(2, "RM Deutschrap", "https://rautemusik.stream43.radiohost.de/rm-deutschrap-charts_mp3-192");
    g_stations.emplace_back(3, "RM Charthits", "https://rautemusik.stream43.radiohost.de/charthits");
    g_stations.emplace_back(4, "bigFM Deutschrap", "https://stream.bigfm.de/oldschooldeutschrap/aac-128");
    g_stations.emplace_back(5, "BreakZ", "https://rautemusik.stream43.radiohost.de/breakz");
    g_stations.emplace_back(6, "1.fm DeepHouse", "http://strm112.1.fm/deephouse_mobile_mp3");
    g_stations.emplace_back(7, "1.fm Dance", "https://strm112.1.fm/dance_mobile_mp3");
    g_stations.emplace_back(8, "104.6rtl Dance Hits", "https://stream.104.6rtl.com/dance-hits/mp3-128/konsole");
    g_stations.emplace_back(9, "Absolut.de Hot", "https://edge22.live-sm.absolutradio.de/absolut-hot");
    g_stations.emplace_back(10, "Sunshine Live - EDM", "http://stream.sunshine-live.de/edm/mp3-192/stream.sunshine-live.de/"); // edm, new, german
    g_stations.emplace_back(11, "PulseEDM Dance Radio", "http://pulseedm.cdnstream1.com:8124/1373_128.m3u"); // edm, new (mpv handles .m3u)

    if (g_stations.empty()) {
        std::cerr << "No radio stations defined. Exiting." << std::endl;
        return 1;
    }
    g_active_station_idx = 0;

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100);

    for (size_t i = 0; i < g_stations.size(); ++i) {
        RadioStream& station = g_stations[i];
        station.mpv = mpv_create();
        if (!station.mpv) {
            endwin();
            std::cerr << "Failed to create MPV instance for " << station.name << std::endl;
            g_quit_flag = true;
            for(size_t k=0; k<i; ++k) if(g_stations[k].mpv) mpv_terminate_destroy(g_stations[k].mpv);
            return 1;
        }

        check_mpv_error(mpv_initialize(station.mpv), "mpv_initialize for " + station.name);

        mpv_set_property_string(station.mpv, "vo", "null");
        mpv_set_property_string(station.mpv, "audio-display", "no");
        mpv_set_property_string(station.mpv, "input-default-bindings", "no");
        mpv_set_property_string(station.mpv, "input-vo-keyboard", "no");
        mpv_set_property_string(station.mpv, "terminal", "no");
        mpv_set_property_string(station.mpv, "msg-level", "all=warn");

        check_mpv_error(mpv_observe_property(station.mpv, station.id, "media-title", MPV_FORMAT_STRING), "observe media-title for " + station.name);
        check_mpv_error(mpv_observe_property(station.mpv, station.id, "eof-reached", MPV_FORMAT_FLAG), "observe eof-reached for " + station.name);

        const char *cmd[] = {"loadfile", station.url.c_str(), "replace", nullptr};
        check_mpv_error(mpv_command_async(station.mpv, 0, cmd), "loadfile for " + station.name);

        // Set initial volume based on whether this is the active station
        double initial_volume = (static_cast<int>(i) == g_active_station_idx) ? 100.0 : 0.0;
        station.current_volume = initial_volume;
        station.target_volume = initial_volume;
        mpv_set_property_async(station.mpv, 0, "volume", MPV_FORMAT_DOUBLE, &initial_volume);
        station.current_title = "Connecting...";
    }

    std::thread mpv_thread(mpv_event_loop);

    g_needs_redraw = true;
    while (!g_quit_flag) {
        if (g_needs_redraw && !g_quit_flag) {
            redraw_ui();
        }

        int ch = getch();

        if (ch != ERR) {
            g_needs_redraw = true;
            if (ch == KEY_UP) {
                if (g_active_station_idx > 0) {
                    switch_station(g_active_station_idx - 1);
                }
            } else if (ch == KEY_DOWN) {
                if (g_active_station_idx < static_cast<int>(g_stations.size()) - 1) {
                    switch_station(g_active_station_idx + 1);
                }
            } else if (ch == 'q' || ch == 'Q') {
                g_quit_flag = true;
                break;
            } else {
                g_needs_redraw = false;
            }
        }
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    g_quit_flag = true;

    for (RadioStream& station : g_stations) {
        if (station.mpv) {
            const char *quit_cmd[] = {"quit", nullptr};
            mpv_command_async(station.mpv, 0, quit_cmd);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (mpv_thread.joinable()) {
        mpv_thread.join();
    }

    for (RadioStream& station : g_stations) {
        if (station.mpv) {
            mpv_terminate_destroy(station.mpv);
            station.mpv = nullptr;
        }
    }

    if (stdscr != NULL && !isendwin()) {
      clear();
      mvprintw(0,0, "Exiting radio switcher...");
      refresh();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      endwin();
    }

    return 0;
}