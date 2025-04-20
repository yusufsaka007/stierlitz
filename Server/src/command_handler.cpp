#include "command_handler.hpp"

Command::Command(const std::string& __cmd, const std::string& __description) {
    cmd = __cmd;
    description = __description;
}

CommandHandler::CommandHandler(ClientHandler*** __p_clients, EventLog* __p_event_log, const std::atomic<bool>* __p_shutdown_flag) {
    p_clients_ = __p_clients;
    p_event_log_ = __p_event_log;
    p_shutdown_flag_ = __p_shutdown_flag;

    command_list_.emplace_back("help", "Display help message or information about a specific command. help <command>");
    command_list_.emplace_back("exit", "Exit the program. Can be used with -k flag")
    command_list_.emplace_back("list", "List all connected clients");
}

int CommandHandler::parse_command(const std::string& __cmd) {
   return; 
}

void CommandHandler::execute_command(const std::string& __cmd) {
    int rc = parse_command(__cmd);
    if (rc == -1) {
        *p_event_log_ << LOG_MUST << RED << "Invalid command: " << __cmd << RESET;
        return;
    }
}