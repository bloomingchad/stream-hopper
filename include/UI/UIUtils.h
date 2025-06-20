#ifndef UIUTILS_H
#define UIUTILS_H

#include <string>
#include <ncurses.h>

std::string truncate_string(const std::string& str, size_t width);
std::string format_history_timestamp(const std::string& ts_str);
void draw_box(int y, int x, int w, int h, const std::string& title, bool is_focused);
bool contains_ci(const std::string& haystack, const std::string& needle);

#endif // UIUTILS_H
