"""Print every message on an MQTT topic (like the Node-RED debug tab) to see whether the board really sends.

Usage:  python mqtt_listen.py [topic] [-u USERNAME] [-p PASSWORD] [--url URL] [-s SECONDS]
  topic  default CENEXTGEN3/#   (# = every topic under this name, e.g. CENEXTGEN3/mygroup/#)
  --url  default mqtt://mqtt.ce-esl.com:1883 (wss://mqtt.ce-esl.com:443/mqtt also works)
  -s     stop after this many seconds, default 0 = until Ctrl+C
"""
import argparse
import os
import socket
import struct
import sys
import time

from mqtt_check import DEFAULT_URL, CheckFailed, ask_login, mqtt_connect, open_transport, read_packet, subscribe


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser(description="Print every message on an MQTT topic.")
    ap.add_argument("topic", nargs="?", default="CENEXTGEN3/#", help="topic to listen to (default CENEXTGEN3/#)")
    ap.add_argument("-u", "--user", help="MQTT username")
    ap.add_argument("-p", "--password", help="MQTT password (asked when it runs if left out)")
    ap.add_argument("--url", default=DEFAULT_URL, help=f"broker url (default {DEFAULT_URL})")
    ap.add_argument("-s", "--seconds", type=int, default=0, help="stop after this many seconds (default: until Ctrl+C)")
    args = ap.parse_args()

    user, password = ask_login(args.user, args.password)
    try:
        conn = open_transport(args.url, log=lambda m: None)
        mqtt_connect(conn, "cenextgen-listen-" + os.urandom(3).hex(), user, password)
        subscribe(conn, args.topic)
    except CheckFailed as e:
        sys.exit(f"[FAIL] {e}\nRun mqtt_check.py to see which step fails.")
    print(f"Listening to {args.topic} on {args.url}"
          + (f" for {args.seconds} s" if args.seconds else " (Ctrl+C to stop)") + "...", flush=True)

    end = time.time() + args.seconds if args.seconds else float("inf")
    last_ping = time.time()
    try:
        while time.time() < end:
            if time.time() - last_ping > 20:
                conn.send(b"\xc0\x00")  # PINGREQ so the broker does not drop a quiet connection
                last_ping = time.time()
            try:
                ptype, data = read_packet(conn)
            except socket.timeout:
                continue
            if ptype == 3:
                tlen = struct.unpack("!H", data[:2])[0]
                topic = data[2:2 + tlen].decode("utf-8", "replace")
                payload = data[2 + tlen:].decode("utf-8", "replace")
                print(f"{time.strftime('%H:%M:%S')}  {topic}  ->  {payload}", flush=True)
    except KeyboardInterrupt:
        pass
    except ConnectionError as e:
        sys.exit(f"[FAIL] {e}")
    print("Stopped")


if __name__ == "__main__":
    main()
