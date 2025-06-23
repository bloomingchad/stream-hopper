#include "Utils.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept> // Required for std::runtime_error

void check_mpv_error(int status, const std::string& context) {
    if (status < 0) {
        std::string error_msg = "MPV Error (" + context + "): " + mpv_error_string(status);
        throw std::runtime_error(error_msg);
    }
}

// Updated to use the UrlEncodingStyle enum
std::string url_encode(const std::string& value, UrlEncodingStyle encoding_style) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (encoding_style == UrlEncodingStyle::BANDCAMP_SPECIAL) {
            if (c == ' ' || c == '-' || c == '.') {
                escaped << '+';
                continue;
            }
        }

        if (isalnum(c) || c == '_' || c == '~' || (encoding_style != UrlEncodingStyle::BANDCAMP_SPECIAL && c == '.')) {
            if (encoding_style == UrlEncodingStyle::BANDCAMP_SPECIAL &&
                c == '-') { // already handled above but defensive
                escaped << '+';
            } else {
                escaped << c;
            }
            continue;
        }

        if (c == ' ') {
            if (encoding_style == UrlEncodingStyle::QUERY_PLUS) {
                escaped << '+';
            } else { // PATH_PERCENT
                escaped << "%20";
            }
            continue;
        }

        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char) c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

bool execute_open_command(const std::string& url, std::string& error_message) {
    std::string open_command;
    bool is_termux = (getenv("TERMUX_VERSION") != nullptr);

    if (is_termux) {
        open_command = "termux-open";
    } else {
        open_command = "xdg-open";
    }

    std::string check_cmd = "command -v " + open_command + " >/dev/null 2>&1";
    if (system(check_cmd.c_str()) != 0) {
        error_message = "[ERROR] '" + open_command + "' not found.";
        if (is_termux) {
            error_message += " Please run: pkg install termux-tools";
        }
        return false;
    }

    std::string full_cmd = open_command + " '" + url + "' &";
    system(full_cmd.c_str());
    return true;
}
