//g++ radio.cpp -o radio $(pkg-config --cflags --libs mpv ncurses) -pthread
//Radio Switcher - Discovery mode

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib> // For getenv
#include <cstring> // For strcmp

#include <mpv/client.h>
#include <ncurses.h>

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

    RadioStream(int unique_id, const std::string& n, const std::string& u)
        : mpv(nullptr), url(u), name(n), current_title("Loading..."), id(unique_id) {}
};

std::vector<RadioStream> g_stations;
int g_active_station_idx = 0;
bool g_needs_redraw = true;
volatile bool g_quit_flag = false;

void redraw_ui() {
    if (g_quit_flag) return;
    clear();
    mvprintw(0, 0, "Radio Switcher - Press Q to quit");
    for (size_t i = 0; i < g_stations.size(); ++i) {
        if (static_cast<int>(i) == g_active_station_idx) {
            attron(A_REVERSE);
        }
        mvprintw(2 + i, 2, "[%s] %s: %s",
                 (static_cast<int>(i) == g_active_station_idx) ? "Playing" : "Muted  ",
                 g_stations[i].name.c_str(),
                 g_stations[i].current_title.c_str());
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
    }
}


int main(int argc, char *argv[]) {
    g_stations.emplace_back(0, "ILoveRadio", "https://ilm.stream18.radiohost.de/ilm_iloveradio_mp3-192");
    g_stations.emplace_back(1, "ILove2Dance", "https://ilm.stream18.radiohost.de/ilm_ilove2dance_mp3-192");
    g_stations.emplace_back(2, "RM Deutschrap", "https://rautemusik.stream43.radiohost.de/rm-deutschrap-charts_mp3-192");
    g_stations.emplace_back(3, "RM Charthits", "https://rautemusik.stream43.radiohost.de/charthits");

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

        int mute_flag = (static_cast<int>(i) == g_active_station_idx) ? 0 : 1;
        mpv_set_property_async(station.mpv, 0, "mute", MPV_FORMAT_FLAG, &mute_flag);
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
                    int mute_on = 1;
                    if(g_stations[g_active_station_idx].mpv) mpv_set_property_async(g_stations[g_active_station_idx].mpv, 0, "mute", MPV_FORMAT_FLAG, &mute_on);
                    g_active_station_idx--;
                    int mute_off = 0;
                    if(g_stations[g_active_station_idx].mpv) mpv_set_property_async(g_stations[g_active_station_idx].mpv, 0, "mute", MPV_FORMAT_FLAG, &mute_off);
                }
            } else if (ch == KEY_DOWN) {
                if (g_active_station_idx < static_cast<int>(g_stations.size()) - 1) {
                    int mute_on = 1;
                    if(g_stations[g_active_station_idx].mpv) mpv_set_property_async(g_stations[g_active_station_idx].mpv, 0, "mute", MPV_FORMAT_FLAG, &mute_on);
                    g_active_station_idx++;
                    int mute_off = 0;
                    if(g_stations[g_active_station_idx].mpv) mpv_set_property_async(g_stations[g_active_station_idx].mpv, 0, "mute", MPV_FORMAT_FLAG, &mute_off);
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