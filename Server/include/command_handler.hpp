#ifndef COMMAND_HANDLER_HPP
#define COMMAND_HANDLER_HPP

#include <iostream>
#include <string>
#include <vector>
#include "server_macros.hpp"

//Think of a better way
constexpr char* INDEX_FLAG = "-i";
constexpr char* HELP_FLAG = "-h";

class ClientHandler; // Forward declaration

struct Command {
    std::string cmd;
    std::string description;
    std::vector<const char*> reguired_args; 
    Command(const std::string& __cmd, const std::string& __description);
};

class CommandHandler {
public:
    CommandHandler();
    int get_command();
    int parse_command();
    int execute_command();
    
    int command_help(const char* __command);
    int command_exit();
    int command_list_clients();
    int info_client(const int& index=selected_client_);
    int kill_client(const int& index=selected_client_);
    int select_client(const int& index);

    int send_command();

private:
    std::string command_;
    std::string error_;
    ClientHandler** clients_;
    int selected_client_;
    std::vector<Command> command_list_;
};

#endif // COMMAND_HANDLER_HPP