#include "Utils.h"
#include <iostream>
#include <stdexcept> // Required for std::runtime_error

void check_mpv_error(int status, const std::string& context) {
    if (status < 0) {
        std::string error_msg = "MPV Error (" + context + "): " + mpv_error_string(status);
        // Instead of exiting, throw a standard C++ exception.
        // This allows the call stack to unwind and destructors to be called.
        throw std::runtime_error(error_msg);
    }
}
