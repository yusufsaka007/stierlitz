#ifndef DEBUG_HPP
#define DEBUG_HPP

#ifdef DEBUG_MODE
    #include <cstdio>
    #define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(...) ((void) 0)
#endif

#endif // DEBUG_HPP