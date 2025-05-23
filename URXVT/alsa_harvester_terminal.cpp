#include <alsa/asoundlib.h>
#include "tunnel_terminal_include.hpp"

volatile bool shutdown_flag_ = false;

void handle_sigint(int) {
    shutdown_flag_ = true;
}


int main(int argc, char* argv[]) {
    // Ascii 
    std::cout << "\n\n                                   _       _____                           \n                             /\\   | |     / ____|  /\\\n                            /  \\  | |    | (___   /  \\\n                           / /\\ \\ | |     \\___ \\ / /\\ \\\n                          / ____ \\| |____ ____) / ____ \\\n        _    _          _/_/___ \\_\\______|_____/_/__ _\\_\\___ ______ _____  \n       | |  | |   /\\   |  __ \\ \\    / /  ____|/ ____|__   __|  ____|  __ \\\n       | |__| |  /  \\  | |__) \\ \\  / /| |__  | (___    | |  | |__  | |__) |\n       |  __  | / /\\ \\ |  _  / \\ \\/ / |  __|  \\___ \\   | |  |  __| |  _  / \n       | |  | |/ ____ \\| | \\ \\  \\  /  | |____ ____) |  | |  | |____| | \\ \\\n       |_|  |_/_/    \\_\\_|  \\_\\  \\/   |______|_____/   |_|  |______|_|  \\_\\\n                                                                           \n                                                                           \n\n    " << std::endl;

    std::signal(SIGINT, handle_sigint);
    std::string fifo_name = argv[1];
    std::string hw_out = argv[2];
    std::string out_name = argv[3];

    std::filesystem::create_directories("fifo");
    if (!std::filesystem::exists(fifo_name)) {
        if (mkfifo(fifo_name.c_str(), 0666) == -1) {
            std::cerr << RED << "[-] Error while creating the fifo" << RESET << std::endl;
            return -1;
        }
    }

    int fd = open(fifo_name.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << RED << "[-] Error while opening the fifo path: " << fifo_name << RESET << std::endl;
        return -1;
    }

    std::cout << "[+] Using " << hw_out;
    std::cout << "\n[+] Connected to the output FIFO: " << fifo_name << "\n";
    std::cout << "\n\n====================Listening For Samples====================\n\n";

    std::cin.get();

    close(fd);
    std::filesystem::remove(fifo_name);

    return 0;
}