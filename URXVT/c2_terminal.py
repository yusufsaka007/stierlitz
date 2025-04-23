#!/usr/bin/env python3
import os
from colorama import Fore, Style, init
import time

init(autoreset=True)

def open_fifo(fifo_path, mode):
    # Wait until there is a reader
    while True:
        try:
            fifo_fd = os.open(fifo_path, mode | os.O_NONBLOCK)
            return os.fdopen(fifo_fd, 'r' if mode == os.O_RDONLY else 'w')
        except OSError as e:
            if e.errno == 6 or e.errno == 32:
                print(f"{Fore.YELLOW}[!] Server command_handler not connected, retrying...")
                time.sleep(3)
            else:
                raise

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))

    fifo_dir = os.path.join(script_dir, "fifo")
    os.makedirs(fifo_dir, exist_ok=True)

    in_fifo_path = os.path.join(fifo_dir, "c2_in_fifo")
    if not os.path.exists(in_fifo_path):
        os.mkfifo(in_fifo_path)
    out_fifo_path = os.path.join(fifo_dir, "c2_out_fifo")
    if not os.path.exists(out_fifo_path):
        os.mkfifo(out_fifo_path)

    print(f"{Fore.GREEN}[+] Writing command input to {in_fifo_path}")
    print(f"{Fore.GREEN}[+] Reading command output from {out_fifo_path}")
    
    in_fifo = open_fifo(in_fifo_path, os.O_WRONLY)
    print(f"{Fore.GREEN}[+] Connected to {in_fifo_path}")
    out_fifo = open_fifo(out_fifo_path, os.O_RDONLY)
    print(f"{Fore.GREEN}[+] Connected to {out_fifo_path}")

    while True:
        try:
            command = input(f"{Fore.CYAN}stierlitz> {Fore.RESET}")
            in_fifo.write(command + '\n')
            in_fifo.flush()
            if command == "exit":
                print(f"{Fore.GREEN}[+] Exiting...")
                break
            response = out_fifo.readline().strip()
            if response == b"":
                print(f"{Fore.RED}[-] No response from server.")
            else:
                print(response, flush=True)
        except BrokenPipeError:
            print(f"{Fore.RED}[-] Broken pipe: Reader disconnected.")
            in_fifo.close()
            in_fifo = open_fifo(in_fifo_path, os.O_WRONLY)
        except KeyboardInterrupt:
            print(f"{Fore.RED}[-] Interrupted by user")
            break
    if in_fifo:
        in_fifo.close()