#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <mpv/client.h>
#include <ncurses.h>

// A single, globally accessible error checker to be used across the project.
void check_mpv_error(int status, const std::string& context);

enum class UrlEncodingStyle {
    QUERY_PLUS,
    PATH_PERCENT,
    BANDCAMP_SPECIAL,
    UNKNOWN // Fallback for safety
};

std::string url_encode(const std::string& value, UrlEncodingStyle encoding_style);
bool execute_open_command(const std::string& url, std::string& error_message);

#endif // UTILS_H
