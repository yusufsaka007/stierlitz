#include <alsa/asoundlib.h>
#include "tunnel_terminal_include.hpp"

#define MAX_BAR_LENGTH 50

int buffer_frames;
unsigned int sample_rate;
int channels_local;
int channels_target;
char* buffer = nullptr;

int tunnel_fifo_data = -1;
int tunnel_fifo_in = -1;

volatile bool shutdown_flag_ = false;

std::string fifo_data_name;
std::string fifo_in_name;
std::string hw_out;

snd_pcm_t* playback_handle = nullptr;
snd_pcm_hw_params_t* playback_params = nullptr;
snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;

void handle_sigint(int) {
    shutdown_flag_ = true;
}

int get_local_channels() {
    int rc;
    unsigned int min_channels;
    unsigned int max_channels;

    snd_pcm_hw_params_get_channels_min(playback_params, &min_channels);
    snd_pcm_hw_params_get_channels_max(playback_params, &max_channels);

    for (int i=min_channels;i<=max_channels;i++) {
        if ((rc = snd_pcm_hw_params_test_channels(playback_handle, playback_params, i)) == 0) {
            if (i == 1) { // Mono is supported
                channels_local = i;
            } else if (i == 2) { // But stereo is preferred
                channels_local = i;
            }
        }
    }
    
    if (channels_local == 0) {
        return -1;
    }

    /*ToDo add mono <-> stereo conversion on future*/
    if (channels_local != channels_target) {
        return -1;
    }

    return 0;
}
int get_target_channels() {
    char initial_arg[256];
    fd_set init_fds;
    FD_ZERO(&init_fds);
    FD_SET(tunnel_fifo_data, &init_fds);

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    int rc = select(tunnel_fifo_data + 1, &init_fds, nullptr, nullptr, &tv); 
    if (rc < 0) {
        std::cerr << RED << "[-] Error during select " << strerror(errno) << RESET << std::endl; 
        return -1;
    } else if (rc == 0) {
        std::cerr << RED << "[-] Timeout occured during select " << RESET << std::endl; 
        return -1;
    }

    rc = read(tunnel_fifo_data, initial_arg, sizeof(initial_arg));
    if (rc == sizeof(int)) {
        memcpy(&channels_target, initial_arg, sizeof(int));
        if (channels_target < 0) {
            // Target failed to initialize it's recorder and sent the error code instead
            std::cerr << RED << "[-] Target failed to initialize it's playback device: " << snd_strerror(channels_target) << RESET << std::endl; 
            if (channels_target == -ENOTSUP) {
                std::cerr << RED << "[-] Target does not support mono or stereo capture" << RESET << std::endl;         
            }
            return -1;
        }
        else if(channels_target > 0 && channels_target <= 2) {
            std::cout << GREEN << "[+] Target initialization successful. Starting to harvest ";
            if (channels_target == 2) {
                std::cout << "stereo sound...";
            } else if (channels_target == 1) {
                std::cout << "mono sound...";
            }
            std::cout << RESET << std::endl;
            return 0;
        } else {
            std::cerr << RED << "[-] Corrupted data received from target" << RESET << std::endl; 
            return -1;
        }
    }

    std::cerr << RED << "[-] Error received from server " << initial_arg << RESET << std::endl; 
    return -1;

}

int alsa_playback_init() {
    int err;
    if ((err = snd_pcm_open(&playback_handle, hw_out.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        std::cerr << RED << "[-] Failed to open playback device: " << snd_strerror(err) << RESET << std::endl;
        return -1;
    }
    if ((err = snd_pcm_hw_params_malloc(&playback_params)) < 0) {
        std::cerr << RED << "[-] Failed to allocate playback params: " << snd_strerror(err) << RESET << std::endl;
        return -1;
    }
    if ((err = snd_pcm_hw_params_any(playback_handle, playback_params)) < 0) {
        std::cerr << RED << "[-] Failed to initialize playback parameter structure: " << snd_strerror(err) << RESET << std::endl;
        return -1;
    }
    if ((err = snd_pcm_hw_params_set_access(playback_handle, playback_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        std::cerr << RED << "[-] Failed to set playback access: " << snd_strerror(err) << RESET << std::endl;
        return -1;
    }
    if ((err = snd_pcm_hw_params_set_format(playback_handle, playback_params, format)) < 0) {
        std::cerr << RED << "[-] Failed to set format: " << snd_strerror(err) << RESET << std::endl;
        return -1;
    }
    if ((err = snd_pcm_hw_params_set_rate_near(playback_handle, playback_params, &sample_rate, 0)) < 0) {
        std::cerr << RED << "[-] Failed to set sample rate: " << snd_strerror(err) << RESET << std::endl;
        return -1;
    }
    if ((err = get_local_channels()) < 0) {
        std::cerr << RED << "[-] Stereo is not supported! Please choose another input device" << RESET << std::endl;
        return -1;
    }
    if ((err = snd_pcm_hw_params_set_channels(playback_handle, playback_params, channels_local)) < 0) {
        std::cerr << RED << "[-] Failed to set channels: " << snd_strerror(err) << RESET << std::endl;
        return -1;
    }
    if ((err = snd_pcm_hw_params(playback_handle, playback_params)) < 0) {
        std::cerr << RED << "[-] Failed to set parameters" << snd_strerror(err) << RESET << std::endl;
        return -1;
    }

    snd_pcm_hw_params_free(playback_params);
    playback_params = nullptr;

    if ((err = snd_pcm_prepare(playback_handle)) < 0) {
        std::cerr << RED << "[-] Failed to prepare playback interface: " << snd_strerror(err) << RESET << std::endl;
        return -1;
    }
    
    std::cout << GREEN << "[+] Initialization successful" << RESET << std::endl;
    return 0;
}

void alsa_playback_cleanup() {
    if (playback_handle != nullptr) {
        snd_pcm_drain(playback_handle);
        snd_pcm_close(playback_handle);
        playback_handle = nullptr;
    }
    if (playback_params != nullptr) {
        snd_pcm_hw_params_free(playback_params);
        playback_params = nullptr;
    }
}
void alsa_harvester_cleanup() {
    if (tunnel_fifo_data != -1) {
        close(tunnel_fifo_data);
        tunnel_fifo_data = -1;
    } 
    if (tunnel_fifo_in != -1) {
        close(tunnel_fifo_in);
        tunnel_fifo_in = -1;
    }
    if (std::filesystem::exists(fifo_data_name)) {
        std::filesystem::remove(fifo_data_name);
    }
    if (std::filesystem::exists(fifo_in_name)) {
        std::filesystem::remove(fifo_in_name);
    }

    if (buffer != nullptr) {
        delete[] buffer;
        buffer = nullptr;
    }
}

void draw_bar(int amplitude) {
    int bar_length = (amplitude * MAX_BAR_LENGTH + 16384) / 32768;

    printf("\r[+] Amplitude: %d\n", amplitude);
    printf("\r[");
    for (int i=0;i<bar_length;i++) {
        printf("#");
    }
    for (int i=bar_length;i<MAX_BAR_LENGTH;i++) {
        printf(" ");
    }
    printf("]\n");
    fflush(stdout);
}

void harvest() {
    fd_set read_fds;
    int bytes_per_sample = snd_pcm_format_width(format) / 8;
    int bytes_per_frame = bytes_per_sample * channels_target;
    buffer = new char[buffer_frames * bytes_per_frame];
    int err;
    int old_avg = 0.0f;
    int new_avg;
    float avg;
    int16_t* samples;
    int sum = 0;
    size_t total;
    size_t expected = buffer_frames * bytes_per_frame;
    ssize_t bytes_read;
    int rc;

    while (!shutdown_flag_) {
        FD_ZERO(&read_fds);
        FD_SET(tunnel_fifo_data, & read_fds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        rc = select(tunnel_fifo_data + 1, &read_fds, nullptr, nullptr, &timeout);
        if (rc > 0 && FD_ISSET(tunnel_fifo_data, &read_fds)) {
            total = 0;

            while (total < expected) {
                bytes_read = read(tunnel_fifo_data, buffer + total, expected - total);
                if (bytes_read <= 0) {
                    std::cerr << RED << "Error while reading audio sample: " << strerror(errno) << RESET << std::endl;
                    shutdown_flag_ = true;
                    break;
                }
                total += bytes_read;
            }

            if (!shutdown_flag_) {
                if (total > 128) {
                    // Where the magic happens
                    err = snd_pcm_writei(playback_handle, buffer, buffer_frames);
                    if (err < 0) {
                        snd_pcm_prepare(playback_handle); // Recover from buffer underrun
                        snd_pcm_writei(playback_handle, buffer, buffer_frames);
                    }

                    // Audio visualizer
                    samples = (int16_t*) buffer;
                    sum = 0;
                    for (int i=0;i<buffer_frames*channels_local;i++) {
                        sum += abs(samples[i]);
                    }

                    new_avg = sum / (buffer_frames * channels_local);
                    avg = 0.8f * old_avg + 0.2f * new_avg;
                    draw_bar((int) avg);
                    printf("\033[2A");
                    old_avg = avg;
                    continue;
                }
                buffer[total] = '\0';
                std::string line(buffer);
                if (line.find("[__end__]") != std::string::npos) {
                    std::cout << "\n" << MAGENTA << "[__end__]" << RESET << std::endl;;
                    break;
                }
                if (line.find("[__error__]") != std::string::npos) {
                    std::cout << "\n" << RED << line << RESET << std::endl;
                    break;
                }
            } else {
                std::cout << "\n" << RED << "Error occured while reading" << RESET << std::endl;
                break;
            }
        } else if (rc < 0) {
            std::cerr << RED << "An error occurred while reading from fifo: " << strerror(errno) << RESET << std::endl;
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    // Ascii 
    std::cout << "\n\n                                   _       _____                           \n                             /\\   | |     / ____|  /\\\n                            /  \\  | |    | (___   /  \\\n                           / /\\ \\ | |     \\___ \\ / /\\ \\\n                          / ____ \\| |____ ____) / ____ \\\n        _    _          _/_/___ \\_\\______|_____/_/__ _\\_\\___ ______ _____  \n       | |  | |   /\\   |  __ \\ \\    / /  ____|/ ____|__   __|  ____|  __ \\\n       | |__| |  /  \\  | |__) \\ \\  / /| |__  | (___    | |  | |__  | |__) |\n       |  __  | / /\\ \\ |  _  / \\ \\/ / |  __|  \\___ \\   | |  |  __| |  _  / \n       | |  | |/ ____ \\| | \\ \\  \\  /  | |____ ____) |  | |  | |____| | \\ \\\n       |_|  |_/_/    \\_\\_|  \\_\\  \\/   |______|_____/   |_|  |______|_|  \\_\\\n                                                                           \n                                                                           \n\n    " << std::endl;

    std::signal(SIGINT, handle_sigint);
    
    fifo_data_name = argv[1];
    fifo_in_name = fifo_data_name + (std::string) "_in";
    
    hw_out = argv[2];
    buffer_frames = std::stoi(argv[3]);
    sample_rate = std::stoi(argv[4]);
    std::string out_name = argv[5];

    int tries = 0;
    int is_success = 0;

    std::filesystem::create_directories("fifo");
    if (!std::filesystem::exists(fifo_data_name)) {
        if (mkfifo(fifo_data_name.c_str(), 0666) == -1) {
            std::cerr << RED << "[-] Error while creating the data fifo" << RESET << std::endl;
            goto cleanup;
        }
    }
    if (!std::filesystem::exists(fifo_in_name)) {
        if (mkfifo(fifo_in_name.c_str(), 0666) == -1) {
            std::cerr << RED << "[-] Error while creating the input fifo" << RESET << std::endl;
            goto cleanup;
        }
    }

    tunnel_fifo_data = open(fifo_data_name.c_str(), O_RDONLY);
    if (tunnel_fifo_data < 0) {
        std::cerr << RED << "[-] Error while opening the fifo path: " << fifo_data_name << RESET << std::endl;
        goto cleanup;
    }
    std::cout << GREEN << "[+] Connected to the data fifo " << fifo_data_name.substr(fifo_data_name.find_last_of('/') + 1) << RESET << std::endl;

    while ((tunnel_fifo_in = open(fifo_in_name.c_str(), O_WRONLY)) < 0 && tries++ <= 20) {
        std::cout << YELLOW << "[!] Waiting reader end to connect to the fifo..." << RESET << std::endl;
        usleep(100000);
    }
    if (tunnel_fifo_in < 0) {
        std::cerr << RED << "[-] Failed to connect to the input fifo. " << fifo_in_name << " Terminating the program..." << RESET << std::endl;
        goto cleanup;
    }
    std::cout << GREEN << "[+] Connected to the input fifo " << fifo_in_name.substr(fifo_in_name.find_last_of('/') + 1) << RESET << std::endl;

    std::cout << "[+] Using " << hw_out << std::endl;

    if (get_target_channels() < 0) {
        goto cleanup;
    }

    if (alsa_playback_init() < 0) {
        write(tunnel_fifo_in, &is_success, sizeof(is_success));
        goto cleanup;
    }
    is_success = 1;
    write(tunnel_fifo_in, &is_success, sizeof(is_success));
    close(tunnel_fifo_in); // Not needed anymore
    tunnel_fifo_in = -1;

    std::cout << "\n\n====================Listening For Samples====================\n\n";
    harvest();

cleanup:
    alsa_harvester_cleanup();
    alsa_playback_cleanup();

    return 0;
}