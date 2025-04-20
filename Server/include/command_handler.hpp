#ifndef COMMAND_HANDLER_HPP
#define COMMAND_HANDLER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include "event_logger.hpp"
#include "server_macros.hpp"
#include "client_handler.hpp"

#define COMMAND_NOT_FOUND_ERROR -1
#define COMMAND_ARGUMENT_ERROR -2
#define CLIENT_NOT_FOUND_ERROR -3

struct Argument {
    int arg_num_;
    std::string arg_abbr_;
    std::string arg_full_;
    std::string arg_value_type_;
};

struct Command {
    std::string cmd;
    std::string description;
};

class CommandHandler {
public:
    CommandHandler(
        ClientHandler*** __p_clients, 
        EventLog* __p_event_log,
        const std::atomic<bool>* __p_shutdown_flag
    );
    
    void execute_command(std::string& __cmd);
private:
    int parse_command(const std::string& __cmd);
    int send_command(const std::string& __cmd);
private:
    ClientHandler*** p_clients_;
    EventLog* p_event_log_;
    std::atomic<bool>* const p_shutdown_flag_;
    std::vector<Command> command_list_;
    int selected_client_;
};

#endif // COMMAND_HANDLER_HPP