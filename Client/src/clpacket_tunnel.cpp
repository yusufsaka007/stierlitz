#include "clpacket_tunnel.hpp"

void CLPacketTunnel::run() {
    char file_name[MAX_FILE_NAME + 1] = {0};

    // Receive the file name
    int received = recv(tunnel_socket_, file_name, MAX_FILE_NAME, 0);
    if (received <= 0) {
        printf("[CLPacketTunnel] Error receiving file name: %s\n", strerror(errno));
        return;
    }

    file_name[received] = '\0';
    printf("[CLPacketTunnel::run] File name to send: %s\n", file_name);

    FILE* file = fopen(file_name, "rb");
    if (!file) {
        printf("[CLPacketTunnel::run] Failed to open file: %s\n", strerror(errno));
        return;
    }

    char buffer[BUFFER_SIZE];
    while (!feof(file)) {
        size_t bytes_read = fread(buffer, 1, BUFFER_SIZE, file);
        if (ferror(file)) {
            printf("[CLPacketTunnel::run] File read error\n");
            fclose(file);
            return;
        }

        if (bytes_read > 0) {
            ssize_t total_sent = 0;
            while (total_sent < bytes_read) {
                ssize_t sent = send(tunnel_socket_, buffer + total_sent, bytes_read - total_sent, 0);
                if (sent < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    printf("[CLPacketTunnel::run] Error while sending: %s\n", strerror(errno));
                    fclose(file);
                    return;
                }
                total_sent += sent;
            }
        }
    }
    fclose(file);

    int key_total_sent = 0; // For safety you know
    while (key_total_sent < END_KEY_LEN) {
        int key_sent = send(tunnel_socket_, END_KEY + key_total_sent, END_KEY_LEN - key_total_sent, 0);
        if (key_sent < 0) {
            if (errno == EINTR) {
                continue;
            } 
            printf("[CLPacketTunnel::run] Error sending END_KEY: %s\n", strerror(errno));
            return;
        }
        key_total_sent += key_sent;
    }

    printf("[CLPacketTunnel] File sent successfully\n");
}