#ifndef MACROS_HPP
#define MACROS_HPP

#define DEFAULT_PORT 4242
#define DEFAULT_IP "0.0.0.0"
#define DEFAULT_MAX_CONNECTIONS 5
#define DEFAULT_VERBOSITY 4

#define MAX_BUFFER_SIZE 2048

#define RESET_C2_FIFO "\n[_end_]\033[0m\n"
#define RESET  "\033[0m\n"
#define RESET_NO_NEWLINE "\033[0m"
#define RED "\033[31m[-] "
#define GREEN "\033[32m[+] "
#define YELLOW "\033[33m[!] "
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"

#endif // MACROS_HPP