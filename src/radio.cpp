// src/radio.cpp
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <ncurses.h>
#include "RadioPlayer.h"
#include "Utils.h" // <-- ADDED THIS INCLUDE

// The global check_mpv_error function has been REMOVED from this file.
// It is now included from "Utils.h"

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
