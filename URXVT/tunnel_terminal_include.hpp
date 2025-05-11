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

#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define MAGENTA "\033[35m"
#define YELLOW  "\033[33m"
#define RESET   "\033[0m"