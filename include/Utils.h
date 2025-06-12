#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <mpv/client.h>
#include <ncurses.h>

// A single, globally accessible error checker to be used across the project.
void check_mpv_error(int status, const std::string& context);

#endif // UTILS_H
