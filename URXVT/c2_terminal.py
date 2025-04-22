#!/usr/bin/env python3
import os
from colorama import Fore, Style, init
import time

init(autoreset=True)

def wait_for_fifo(fifo_path):
    # Wait until there is a reader
    while True:
        try:
            fifo_fd = os.open(fifo_path, os.O_WRONLY | os.O_NONBLOCK)
            return os.fdopen(fifo_fd, 'w')
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

    fifo_path = os.path.join(fifo_dir, "c2_fifo")
    if not os.path.exists(fifo_path):
        os.mkfifo(fifo_path)
    print(f"{Fore.GREEN}[+] Writing command input to {fifo_path}")
    
    fifo = wait_for_fifo(fifo_path)

    while True:
        try:
            command = input(f"{Fore.CYAN}stierlitz> {Fore.RESET}")
            fifo.write(command + '\n')
            fifo.flush()
            if command == "exit":
                print(f"{Fore.GREEN}[+] Exiting...")
                break
        except BrokenPipeError:
            print(f"{Fore.RED}[-] Broken pipe: Reader disconnected.")
            fifo.close()
            fifo = wait_for_fifo(fifo_path)
        except KeyboardInterrupt:
            print(f"{Fore.RED}[-] Interrupted by user")
            break
    if fifo:
        fifo.close()