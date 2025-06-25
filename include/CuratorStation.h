#ifndef CURATORSTATION_H
#define CURATORSTATION_H

#include <string>
#include <vector>

// A struct to hold all the rich data for a station being reviewed in Curator Mode.
struct CuratorStation {
    std::string name;
    std::vector<std::string> urls;
    std::string country_code;
    int bitrate = 0;
    int votes = 0;
    std::vector<std::string> tags;
    std::string format = "MP3"; // Added format information
};

#endif // CURATORSTATION_H
