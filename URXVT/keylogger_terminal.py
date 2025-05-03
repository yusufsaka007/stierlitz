#!/usr/bin/env python3

import os
import time
import threading
import select
from colorama import Fore, Style, init
import sys

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    fifo_dir = os.path.join(script_dir, "fifo")
    os.makedirs(fifo_dir, exist_ok=True)

    fifo_path = os.path.join(fifo_dir, "keylogger_fifo")
    if not os.path.exists(fifo_path):
        os.mkfifo(fifo_path)

    fifo = os.open(fifo_path, os.O_RDONLY | os.O_NONBLOCK)
    fifo = os.fdopen(fifo, 'r')
    
    print(f"{Fore.GREEN}[+] Connected to the output FIFO {Style.RESET_ALL}")

    try:
        print(f"{Fore.GREEN}\n\n{'=='*10}Listening For Keylogs{'=='*10}{Style.RESET_ALL}\n\n{Fore.MAGENTA}")
        while True:
            line = ""
            rlist, _, _ = select.select([fifo], [], [], 1)
            if fifo in rlist:
                line = fifo.readline().strip()
                if not line:
                    continue

            if "[__end__]" in line:
                print(f"\n{Fore.MAGENTA}[__end__]{Style.RESET_ALL}\n")
                break
            print(line, end='', flush=True)
    except KeyboardInterrupt:
            print(f"\n{Fore.RED}[!] Exiting...{Style.RESET_ALL}")
    finally:
        fifo.close()
    try:
        input(f"{Fore.YELLOW}[!] Press Enter to exit...{Style.RESET_ALL}")
    except KeyboardInterrupt:
        pass