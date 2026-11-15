# Debug tools

Something does not work? Find the part in the table, then run the tool next to it.

| Problem | Folder | Tool |
|---|---|---|
| A part on the board does not work (RFID, servo, ultrasonic, LED, buzzer, WiFi, MQTT) | `hardware/` | `device_check/device_check.ino` |
| The RFID reader does not read cards | `hardware/` | `rfid_check/rfid_check.ino` |
| Cannot connect to the MQTT broker / unsure if the network blocks it | `network/` | `check_mqtt` (runs `mqtt_check.py`) |
| The board seems to send nothing | `network/` | `mqtt_listen.py` |
| Node-RED is not installed, will not start, or `node-red` is "not found" | `nodered/` | `check_nodered` |

## hardware/ (upload to the ESP32)

Open the `.ino` in Arduino IDE, upload it, then open Serial Monitor at **115200**.

- **device_check**: type a letter and press Enter. `a` = all tests, `r` RFID, `s` servo, `u` ultrasonic, `l` LED, `b` buzzer, `w` WiFi + MQTT. Put the MQTT username / password at the top first.
- **rfid_check**: runs by itself. Press EN to start again. It says which wire to fix, then prints the UID of each card you tap.

## network/ (run on the computer)

`check_mqtt` finds Python (and installs it if it is missing), runs the check, then prints where Python is and the exact commands to run the checks again.

| System | Run |
|---|---|
| Windows | double-click `check_mqtt.cmd`, or `check_mqtt.cmd -u USERNAME` in a terminal |
| Linux / macOS | `bash check_mqtt.sh -u USERNAME` |

The password is asked when it runs (it is not shown while typing). With Python already installed you can also run the scripts directly:

```bash
python mqtt_check.py -u USERNAME                                  # same way as the ESP32 (port 1883)
python mqtt_check.py wss://mqtt.ce-esl.com:443/mqtt -u USERNAME   # same way as MQTTX (WebSocket)
python mqtt_listen.py "CENEXTGEN3/GROUPNAME/#" -u USERNAME         # print every message the board sends
```

`mqtt_check.py` goes step by step (DNS, TCP/TLS, WebSocket, MQTT login, publish and receive) and stops at the first step that fails, saying why.

On Linux / macOS use `python3` if `python` is not found.

## nodered/ (run on the computer)

`check_nodered` checks Node.js, Node-RED and node-red-dashboard, installs what is missing, fixes PATH if needed, then prints the paths and the commands that start Node-RED.

| System | Run |
|---|---|
| Windows | double-click `check_nodered.cmd` |
| Linux / macOS | `bash check_nodered.sh` |

Options: `-Yes` / `--yes` answers yes to every question, `-CheckOnly` / `--check-only` only checks and changes nothing.

What it fixes:

- **Node.js missing or too old**: installs the LTS version (Windows: winget, macOS: Homebrew, Linux: apt / dnf / pacman / zypper). Node-RED 4 needs Node.js 18.5 or newer, Node-RED 5 needs 22.9 or newer; with an older Node.js it installs Node-RED 4.
- **Node-RED missing, or Node-RED 5 on a Node.js that is too old for it** (it installs fine but does not start): installs the version that fits.
- **`node-red` is "not found"** although it is installed: adds the npm folder to PATH (open a new terminal afterwards), and always prints a full-path command that works anyway.
- **Windows PowerShell says "running scripts is disabled"** for `node-red`: use `node-red.cmd` instead (the script tells you when this applies).
- **node-red-dashboard missing**: installs it into `~/.node-red` (restart Node-RED afterwards).
- **Port 1880 busy**: tells you whether Node-RED is already running there or another program uses it (then start Node-RED with `-p 1881`).

Installing on Linux uses `sudo` and asks for your password. If the Linux package of Node.js is too old, the script prints Node-RED's own installer command for Debian, Ubuntu and Raspberry Pi OS.
