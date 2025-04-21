#include "command_handler.hpp"

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

CommandHandler::CommandHandler(ClientHandler*** __p_clients, EventLog* __p_event_log, std::atomic<bool>* __p_shutdown_flag) 
:p_shutdown_flag_ (__p_shutdown_flag)
{
    p_clients_ = __p_clients;
    p_event_log_ = __p_event_log;

    argument_list_.push_back(Argument(INDEX_ARG, ARG_TYPE_INT, "-i", "--index"));
    argument_list_.push_back(Argument(ALL_ARG, ARG_TYPE_SET, "-a", "--all"));
    argument_list_.push_back(Argument(HELP_ARG, ARG_TYPE_SET, "-h", "--help"));
    argument_list_.push_back(Argument(KILL_ARG, ARG_TYPE_SET, "-k", "--kill"));

    command_map_.emplace("help", Command(
        "Show this help message. For specific command help <command> or <command> -h/--help", 
        &CommandHandler::help,
        this,
        {}
    ));
    // command_map_["select"] = Command("select", "Select a client to work with", {}, {INDEX_ARG});
    // command_map_["kill"] = Command("kill", "Kill the selected client. kill -i/--index <client_id> or kill -a/--all for specific clients", {INDEX_ARG, ALL_ARG});
    // command_map_["exit"] = Command("exit", "Shutdown the server. -k/--kill to kill all clients upon exit", {KILL_ARG});
    // command_map_["list"] = Command("list", "List all clients", {});
    // command_map_["info"] = Command("info", "Show information about a client. info -i/--index <client_id>", {}, {INDEX_ARG});
}

bool CommandHandler::command_exists(const std::string& __root_cmd) {
    return command_map_.find(__root_cmd) != command_map_.end();
}

bool CommandHandler::parse_arguments(const std::string& __root_cmd, const std::string& __args_str) {
    // Break the input into individual arguments
    std::istringstream iss(__args_str);
    std::vector<std::string> args((std::istream_iterator<std::string>(iss)), std::istream_iterator<std::string>());

    std::unordered_map<int, std::any> temp_arg_map;
    std::vector<int> found_required_args;

    for (size_t i = 0; i < args.size(); i++) {
        const std::string& arg = args[i];

        auto it = std::find_if(argument_list_.begin(), argument_list_.end(), [&arg](const Argument& a) {
            return arg == a.arg_abbr_ || arg == a.arg_full_;
        });

        if (it == argument_list_.end()) {
            *p_event_log_ << LOG_MUST << RED << "Unknown argument: " << arg << RESET;
            return false;
        }

        int arg_num = it->arg_num_;
        int arg_type = it->arg_value_type_;

        // Track whether required arguments are found
        if (std::find(command_map_.at(__root_cmd).required_.begin(), command_map_.at(__root_cmd).required_.end(), it->arg_num_) != command_map_.at(__root_cmd).required_.end()) {
            found_required_args.push_back(it->arg_num_);
        }

        if (arg_type != ARG_TYPE_SET) {
            if (i + 1 >= args.size()) {
                *p_event_log_ << LOG_MUST << RED << "Missing value for argument: " << arg << RESET;
                return false;
            }

            const std::string& value = args[i+1];

            bool is_next_arg_flag = std::any_of(argument_list_.begin(), argument_list_.end(), [&value](const Argument& a) {
                return value == a.arg_abbr_ || value == a.arg_full_;
            });
        
            if (is_next_arg_flag) {
                *p_event_log_ << LOG_MUST << RED << "Expected value after argument: " << arg << ", but got another argument: " << value << RESET;
                return false;
            }

            try {
                if (arg_type == ARG_TYPE_INT) {
                    temp_arg_map[arg_num] = std::stoi(value);
                } else if (arg_type == ARG_TYPE_STRING) {
                    temp_arg_map[arg_num] = value;
                } else {
                    *p_event_log_ << LOG_MUST << RED << "Unknown argument type for argument: " << arg << RESET;
                    return false;
                }

                i++; // Skip the value since it's already processed
            } catch (const std::exception& e) {
                *p_event_log_ << LOG_MUST << RED << "Invalid value for argument " << arg << ": " << e.what() << RESET;
                return false;
            }
        } else {
            temp_arg_map[it->arg_num_] = true;
        }    
    }

    for (int required_arg : command_map_.at(__root_cmd).required_) {
        if (std::find(found_required_args.begin(), found_required_args.end(), required_arg) == found_required_args.end()) {
            return false;
        }
    }

    arg_map_ = std::move(temp_arg_map);
    return true;
}

int CommandHandler::parse_command(const std::string& __root_cmd) {
    

    if (!command_exists(__root_cmd)) {
        *p_event_log_ << LOG_MUST << RED << "Command not found: " << __root_cmd << ". Type help to see all the available commands" << RESET;
        return -1;
    }

    std::string args_str = "";
    size_t space_pos = cmd_.find(' ');
    if (space_pos != std::string::npos) {
        args_str = cmd_.substr(space_pos + 1); // Extract everything after the first space
    }

    if (!parse_arguments(__root_cmd, args_str)) {
        *p_event_log_ << LOG_MUST << RED << "Invalid command usage " << __root_cmd << "\nhelp " << __root_cmd << " for usage" << RESET;
        return -1;
    }

    return 0;
}

void CommandHandler::help() {
    *p_event_log_ << LOG_MUST << CYAN << "stierlitz Available commands:" << RESET;
    for (const auto& [cmd, command] : command_map_) {
        *p_event_log_ << LOG_MUST << CYAN << cmd << RESET << ": " << command.description_ << "\n";
    }
}

void CommandHandler::cleanup() {
    cmd_.clear();
    arg_map_.clear();
}

void CommandHandler::execute_command() {
    int rc = 0;
    
    std::getline(std::cin, cmd_);
    if (cmd_.empty()) {
        return;
    }

    *p_event_log_ << LOG_MUST << MAGENTA << "[event_log_debug] command is " << cmd_ << RESET;
    std::cout << MAGENTA << "[cout_debug] command is " << cmd_ << RESET << std::endl;

    std::string root_cmd = cmd_.substr(0, cmd_.find(' '));

    rc = parse_command(root_cmd);
    if (rc < 0) {
        return;
    }

    // Execute the command
    Command& command = command_map_.at(root_cmd);
    (command.p_command_handler_->*command.p_command_func_)();

    cleanup();
}

