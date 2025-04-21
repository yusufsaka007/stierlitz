#ifndef COMMAND_HANDLER_HPP
#define COMMAND_HANDLER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <initializer_list>
#include <unordered_map>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <exception>
#include <any>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <future>
#include "event_logger.hpp"
#include "server_macros.hpp"
#include "client_handler.hpp"
#include "data_handler.hpp"

#define INDEX_ARG 1
#define ALL_ARG 2
#define HELP_ARG 3
#define KILL_ARG 4

#define ARG_TYPE_INT 0x1A
#define ARG_TYPE_STRING 0x1B
#define ARG_TYPE_SET 0x1C

class CommandHandler;

struct Argument {
    int arg_num_;
    int arg_value_type_;
    std::string arg_abbr_;
    std::string arg_full_;
    Argument(
        int __arg_num,
        int __arg_value_type,
        const std::string& __arg_abbr,
        const std::string& __arg_full
    );
};

struct Command {
    std::string description_;
    std::vector<int> optional_;
    std::vector<int> required_;
    void (CommandHandler::*p_command_func_)();
    CommandHandler* p_command_handler_;
    Command(
        const std::string& __description,
        void (CommandHandler::*__p_command_func)(),
        CommandHandler* __p_command_handler,
        std::initializer_list<int> __optional,
        std::initializer_list<int> __required = {}
    );
};

class CommandHandler {
public:
    CommandHandler(
        ClientHandler*** __p_clients, 
        EventLog* __p_event_log,
        std::atomic<bool>* __p_shutdown_flag
    );
    void execute_command();
private:
    int parse_command(const std::string& __root_cmd);
    bool command_exists(const std::string& __root_cmd);
    bool parse_arguments(const std::string& __root_cmd, const std::string& __args_str);
    
    void help();
    
    //int send_command();
    void cleanup();
private:
    ClientHandler*** p_clients_;
    EventLog* p_event_log_;
    std::atomic<bool>* p_shutdown_flag_;
    std::unordered_map<std::string,Command> command_map_;
    std::vector<Argument> argument_list_;
    int selected_client_;
    std::string cmd_;
    std::unordered_map<int, std::any> arg_map_;
};

#endif // COMMAND_HANDLER_HPP