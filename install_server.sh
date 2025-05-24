#!/bin/bash
set -e

echo "[*] Installing Python dependencies..."
pip3 install --upgrade pip
pip3 install colorama pyfiglet

echo "[*] Installing C++ dependencies..."
sudo apt update
sudo apt install -y \
    libopencv-dev \
    libasound2-dev \
    libxkbcommon-dev \
    libjpeg-dev \
    rxvt-unicode \
    libtermios-dev || true

echo "[+] Server dependencies installed."