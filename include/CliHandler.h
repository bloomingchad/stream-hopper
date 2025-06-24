#ifndef CLIHANDLER_H
#define CLIHANDLER_H

#include <string>

class CliHandler {
  public:
    CliHandler() = default;
    void handle_list_tags();
    void handle_curate_genre(const std::string& genre);
};

#endif // CLIHANDLER_H
