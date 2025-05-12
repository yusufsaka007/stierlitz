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

#define GREEN     "\033[32m"
#define RED       "\033[31m"
#define MAGENTA   "\033[35m"
#define YELLOW    "\033[33m"
#define BLUE      "\033[34m"
#define CYAN      "\033[36m"
#define RESET     "\033[0m"
#define BG_RED    "\033[41m"
#define BG_GREEN  "\033[42m"
#define BG_YELLOW "\033[43m"
#define BG_BLUE   "\033[44m"
#define BG_CYAN   "\033[46m"
#define BG_RESET  "\033[49m"
#define BG_RED    "\033[41m"
#define BG_GREEN  "\033[42m"
#define BG_YELLOW "\033[43m"
#define BG_BLUE   "\033[44m"
#define BG_CYAN   "\033[46m"
#define BG_RESET  "\033[49m"
#define BOLD      "\033[1m"
#define UNDERLINE "\033[4m"
#define REVERSED  "\033[7m"