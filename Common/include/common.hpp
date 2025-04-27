#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdint>

typedef uint8_t CommandCode;
typedef uint16_t Status;

enum : CommandCode {
    TEST      = 0b00000001,
    KILL      = 0b00000010,
    KEYLOGGER = 0b00000011
};

enum : Status {
    ACCEPTED_OFFER = 0x100,
    REJECTED_OFFER = 0x101,
    EXEC_SUCCESS   = 0x102,
    EXEC_ERROR     = 0x103
};

#define OUT_KEY "$^_out"
#define OUT_KEY_LEN (sizeof(OUT_KEY) - 1)
#define STATUS_SIZE sizeof(Status)
#define OUT_SIZE (OUT_KEY_LEN + STATUS_SIZE)

#define TCP_BASED SOCK_STREAM
#define UDP_BASED SOCK_DGRAM
#define MAX_COMMAND_LEN 256

#endif // COMMON_HPP