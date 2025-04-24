#!/usr/bin/env python3

import os
import time
import threading
import select
from colorama import Fore, Style, init
import sys

init(autoreset=True)

def open_fifo(fifo_path, mode):
    while True:
        try:
            fifo_fd = os.open(fifo_path, mode | os.O_NONBLOCK)
            return os.fdopen(fifo_fd, 'r' if mode == os.O_RDONLY else 'w', buffering=1)
        except OSError as e:
            if e.errno == 6 or e.errno == 32:
                print(f"{Fore.YELLOW}[!] Not synchronized with the Server")
                time.sleep(2)
            else:
                raise
def reader_thread(out_fifo, stop_flag):
    while not stop_flag.is_set():
        rlist, _, _ = select.select([out_fifo], [], [], 0.5)
        if rlist:
            output = ""
            while True:
                line = out_fifo.readline()
                if not line:
                    time.sleep(0.1)
                    continue
                if line:
                    if "[__end__]" in line:
                        output += f"{Fore.MAGENTA}\n[__end__]{Style.RESET_ALL}\n"
                        break
                    output += line  
            if output:
                sys.stdout.write('\r' + ' ' * 80 + '\r') 
                sys.stdout.flush()
                print(f"\n{Fore.MAGENTA}[__out__]\n\n{output}")
                output = ""
            print(f"{Fore.MAGENTA}stierlitz> {Style.RESET_ALL}", end='', flush=True)

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    fifo_dir = os.path.join(script_dir, "fifo")
    os.makedirs(fifo_dir, exist_ok=True)

    in_fifo_path = os.path.join(fifo_dir, "c2_in_fifo")
    out_fifo_path = os.path.join(fifo_dir, "c2_out_fifo")

    if not os.path.exists(in_fifo_path):
        os.mkfifo(in_fifo_path)
    if not os.path.exists(out_fifo_path):
        os.mkfifo(out_fifo_path)

    in_fifo = open_fifo(in_fifo_path, os.O_WRONLY)
    print(f"{Fore.GREEN}[+] Connected to the input FIFO")
    out_fifo = open_fifo(out_fifo_path, os.O_RDONLY)
    print(f"{Fore.GREEN}[+] Connected to the output FIFO")

    stop_flag = threading.Event()
    rt = threading.Thread(target=reader_thread, args=(out_fifo, stop_flag), daemon=True)
    rt.start()

    try:
        while True:
            command = input(f"{Fore.MAGENTA}stierlitz> {Style.RESET_ALL}")
            in_fifo.write(command + '\n')
            in_fifo.flush()

            if command.strip().lower() == "exit":
                print(f"{Fore.YELLOW}[!] Exiting...")
                stop_flag.set()
                break
    except KeyboardInterrupt:
        print(f"{Fore.YELLOW}[!] Exiting...")
        stop_flag.set()
    finally:
        if in_fifo:
            print(f"{Fore.YELLOW}[!] Closing input FIFO")
            in_fifo.close()
        if out_fifo:
            print(f"{Fore.YELLOW}[!] Closing output FIFO")
            out_fifo.close()
        try:
            input(f"{Fore.YELLOW}[!] Press Enter to exit...")
        except KeyboardInterrupt:
            pass