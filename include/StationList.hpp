#ifndef STATIONLIST_HPP
#define STATIONLIST_HPP

#include <vector>
#include <string>
#include <utility>

namespace stream_hopper {

// This is the master list of radio stations.
// To disable a station, simply comment it out using //.
// The order of stations in this list is the order they will appear in the UI.
const std::vector<std::pair<std::string, std::vector<std::string>>> station_data = {
    //{"ILoveRadio", {"https://ilm.stream18.radiohost.de/ilm_iloveradio_mp3-192"}},
    {"ILove2Dance", {
        "https://ilm.stream18.radiohost.de/ilm_ilove2dance_mp3-192",
        "https://streams.ilovemusic.de/iloveradio2-aac.mp3"
    }},
    {"RM Deutschrap", {
        "https://rautemusik.stream43.radiohost.de/rm-deutschrap-charts_mp3-192",
        "http://deutschrap-charts-aacp.rautemusik.fm/listen.pls"
    }},
    {"RM Charthits", {
        "https://rautemusik.stream43.radiohost.de/charthits",
        "http://charthits-aacp.rautemusik.fm/listen.pls"
    }},
    {"bigFM OldSchool Deutschrap", {
        "https://stream.bigfm.de/oldschooldeutschrap/aac-128",
        "https://stream.bigfm.de/oldschooldeutschrap/aac-64"
    }},
    {"bigFM Dance", {
        "https://streams.bigfm.de/bigfm-dance-128-mp3",
        "https://stream.bigfm.de/dance/aac-64"
    }},
    //{"bigFM DanceHall", {"https://audiotainment-sw.streamabc.net/atsw-dancehall-aac-128-5420319"}},
    //{"bigFM GrooveNight", {"https://audiotainment-sw.streamabc.net/atsw-groovenight-aac-128-5346495"}},
    {"BreakZ", {
        "https://breakz-high.rautemusik.fm/stream.mp3",
        "http://breakz-aacp.rautemusik.fm/listen.pls"
    }},
    //{"1.fm DeepHouse", {"http://strm112.1.fm/deephouse_mobile_mp3"}},
    {"1.fm Dance", {"https://strm112.1.fm/dance_mobile_mp3"}},
    {"104.6rtl Dance Hits", {
        "https://stream.104.6rtl.com/dance-hits/mp3-192/konsole",
        "https://stream.104.6rtl.com/dance-hits/mp3-128/konsole",
        "https://stream.104.6rtl.com/dance-hits/aac-64/konsole"
    }},
    //{"Absolut.de Hot", {"https://edge22.live-sm.absolutradio.de/absolut-hot"}},
    {"Sunshine Live - EDM", {
        "http://stream.sunshine-live.de/edm/mp3-192/stream.sunshine-live.de/",
        "http://stream.sunshine-live.de/edm/mp3-128/stream.sunshine-live.de/",
        "http://stream.sunshine-live.de/edm/aac-64/stream.sunshine-live.de/"
    }},
    //{"Sunshine Live - Amsterdam Club", {"https://stream.sunshine-live.de/ade18club/mp3-128"}},
    {"Sunshine Live - Charts", {
        "http://stream.sunshine-live.de/sp6/mp3-192",
        "http://stream.sunshine-live.de/sp6/mp3-128",
        "http://stream.sunshine-live.de/sp6/aac-64"
    }},
    //{"Sunshine Live - EuroDance", {"https://sunsl.streamabc.net/sunsl-eurodance-mp3-192-9832422"}},
    //{"Sunshine Live - Summer Beats", {"http://stream.sunshine-live.de/sp2/aac-64"}},
    {"Kiss FM - German Beats", {
        "https://topradio-stream31.radiohost.de/kissfm-deutschrap-hits_mp3-192",
        "https://topradio-stream31.radiohost.de/kissfm-deutschrap-hits_mp3-128",
        "https://topradio-stream31.radiohost.de/kissfm-deutschrap-hits_aac-64"
    }},
    //{"Kiss FM - DJ Sets", {"https://topradio.stream41.radiohost.de/kissfm-electro_mp3-192"}},
    {"Kiss FM - Remix", {
        "https://topradio.stream05.radiohost.de/kissfm-remix_mp3-192",
        "https://topradio.stream05.radiohost.de/kissfm-remix_mp3-128",
        "https://topradio.stream05.radiohost.de/kissfm-remix_aac-64"
    }},
    //{"Kiss FM - Events", {"https://topradio.stream10.radiohost.de/kissfm-event_mp3-192"}},
    {"Energy - Dance", {"https://edge01.streamonkey.net/energy-dance/stream/mp3"}},
    //{"Energy - MasterMix", {"https://scdn.nrjaudio.fm/adwz1/de/33027/mp3_128.mp3"}},
    {"Energy - Deutschrap", {"https://edge07.streamonkey.net/energy-deutschrap"}},
    {"PulseEDM Dance Radio", {"http://pulseedm.cdnstream1.com:8124/1373_128.m3u"}},
    {"Puls Radio Dance", {
        "https://sc4.gergosnet.com/pulsHD.mp3",
        "http://icecast1.pulsradio.com:8080/pulsAAC64.mp3"
    }},
    {"Puls Radio Club", {"https://str3.openstream.co/2138"}},
    {"Los 40 Dance", {"https://playerservices.streamtheworld.com/api/livestream-redirect/LOS40_DANCE.mp3"}},
    {"RadCap Uplifting", {
        "http://79.111.119.111:8000/upliftingtrance", //320
        "http://79.111.119.111:8002/upliftingtrance" //48
    }},
    {"Regenbogen", {
        "https://audiotainment-sw.streamabc.net/atsw-regenbogen1028-aac-128-5324158", //128
        "https://streams.regenbogen.de/" //64? 128, ?
    }},
    {"RadCap ClubDance", {
        "http://79.111.119.111:8000/clubdance", //320
        "http://79.111.119.111:8002/clubdance" //48
    }},
    {"0nlineradio - Remix", {"https://stream2-0nlineradio.radiohost.de/remix"}},
    {"Radio Scoop - Remix", {
        "https://scoopremix.ice.infomaniak.ch/radioscoop-remix-128.mp3",
        "https://scoopremix.ice.infomaniak.ch/radioscoop-remix-64.aac"
    }},
    {"Mashup FM", {"https://stream.laut.fm/mashupfm"}},
    {"Radio Scoop - Rap", {
        "https://scooprap.ice.infomaniak.ch/radioscoop-rap-128.mp3",
        "https://scooprap.ice.infomaniak.ch/radioscoop-rap-64.aac"
    }},
    {"Cool FM - da Dance", {"https://mediagw.e-tiger.net/stream/dds"}},
    {"Radio Scoop - PowerDance", {
        "https://scooppowerdance.ice.infomaniak.ch/radioscoop-powerdance-128.mp3",
        "https://scooppowerdance.ice.infomaniak.ch/radioscoop-powerdance-64.aac"
    }}
};

} // namespace stream_hopper

#endif // STATIONLIST_HPP
