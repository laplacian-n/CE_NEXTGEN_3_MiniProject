"""Check the MQTT broker step by step - no extra libraries needed.

Usage:  python mqtt_check.py [url] [-u USERNAME] [-p PASSWORD] [-t TOPIC]
  url   mqtt://mqtt.ce-esl.com:1883      default, the same way the ESP32 (PubSubClient) connects
        wss://mqtt.ce-esl.com:443/mqtt   the same way MQTTX connects (MQTT over WebSocket)
  -u    username (asked when it runs if left out, press Enter for none)
  -p    password (asked when it runs, not shown while typing)
  -t    topic for the round-trip test, default CENEXTGEN3/test

Steps:
  1. DNS        can the host name be found
  2. TCP / TLS  can the port be opened (and does TLS work for wss:// and mqtts://)
  3. WebSocket  does the server switch to WebSocket at the path (ws:// and wss:// only)
  4. MQTT       log in, subscribe + publish to the topic, and wait for the message to come back
"""
import argparse
import getpass
import os
import socket
import ssl
import struct
import sys
import time
from base64 import b64encode
from urllib.parse import urlparse

DEFAULT_URL = "mqtt://mqtt.ce-esl.com:1883"
TIMEOUT = 5
DEFAULT_PORTS = {"mqtt": 1883, "mqtts": 8883, "ws": 80, "wss": 443}

CONNACK_CODES = {
    1: "unacceptable protocol version",
    2: "client id rejected",
    3: "server unavailable",
    4: "bad username or password (case-sensitive; with no username, this broker needs one: use -u)",
    5: "not authorized - no such account, or a login is required (use -u)",
}


class CheckFailed(Exception):
    pass


def encode_len(n):
    out = bytearray()
    while True:
        b = n % 128
        n //= 128
        out.append(b | 0x80 if n else b)
        if not n:
            return bytes(out)


def mqtt_str(s):
    b = s.encode("utf-8")
    return struct.pack("!H", len(b)) + b


def packet(ptype, body):
    return bytes([ptype]) + encode_len(len(body)) + body


class Plain:
    """Raw bytes on the socket (mqtt:// mqtts://)"""

    def __init__(self, sock):
        self.sock = sock

    def send(self, data):
        self.sock.sendall(data)

    def recv(self, n):
        chunk = self.sock.recv(n)
        if not chunk:
            raise ConnectionError("the broker closed the connection")
        return chunk


class WebSocket:
    """MQTT wrapped in WebSocket binary frames (ws:// wss://)"""

    def __init__(self, sock):
        self.sock = sock
        self.buf = b""

    def _read(self, n):
        out = b""
        while len(out) < n:
            chunk = self.sock.recv(n - len(out))
            if not chunk:
                raise ConnectionError("the broker closed the connection")
            out += chunk
        return out

    def send(self, data):
        # frames from the client must always be masked
        mask = os.urandom(4)
        n = len(data)
        head = b"\x82" + (bytes([0x80 | n]) if n < 126 else bytes([0x80 | 126]) + struct.pack("!H", n))
        self.sock.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def recv(self, n):
        while not self.buf:
            b0, b1 = self._read(2)
            size = b1 & 0x7F
            if size == 126:
                size = struct.unpack("!H", self._read(2))[0]
            elif size == 127:
                size = struct.unpack("!Q", self._read(8))[0]
            payload = self._read(size)
            op = b0 & 0x0F
            if op == 0x8:
                raise ConnectionError("the broker closed the WebSocket")
            if op in (0x0, 0x2):
                self.buf += payload
        out, self.buf = self.buf[:n], self.buf[n:]
        return out


def recv_exact(conn, n):
    buf = b""
    while len(buf) < n:
        buf += conn.recv(n - len(buf))
    return buf


def read_packet(conn):
    header = recv_exact(conn, 1)[0]
    mult, length = 1, 0
    while True:
        b = recv_exact(conn, 1)[0]
        length += (b & 0x7F) * mult
        if not b & 0x80:
            break
        mult *= 128
    return header >> 4, recv_exact(conn, length)


def parse_url(text):
    url = urlparse(text if "://" in text else "mqtt://" + text)
    if url.scheme not in DEFAULT_PORTS:
        raise CheckFailed(f"unknown {url.scheme}:// - use mqtt, mqtts, ws or wss")
    return url.scheme, url.hostname, url.port or DEFAULT_PORTS[url.scheme], url.path or "/mqtt"


def open_transport(url, log=print):
    """Steps 1-3: DNS, TCP/TLS, WebSocket. Returns a Plain or WebSocket connection."""
    scheme, host, port, path = parse_url(url)
    use_tls = scheme in ("mqtts", "wss")
    use_ws = scheme in ("ws", "wss")

    log("1) DNS")
    try:
        ip = socket.gethostbyname(host)
    except socket.gaierror as e:
        raise CheckFailed(f"cannot find the IP of {host} ({e}) - check the internet connection and the spelling")
    log(f"  [PASS] {host} -> {ip}")

    log("2) TCP" + " / TLS" * use_tls)
    t0 = time.time()
    try:
        sock = socket.create_connection((host, port), timeout=TIMEOUT)
    except OSError as e:
        raise CheckFailed(f"cannot open port {port} ({e}) - wrong port, or this network blocks it")
    log(f"  [PASS] port {port} is open ({(time.time() - t0) * 1000:.0f} ms)")
    sock.settimeout(TIMEOUT)
    if use_tls:
        try:
            sock = ssl.create_default_context().wrap_socket(sock, server_hostname=host)
        except (ssl.SSLError, OSError) as e:
            raise CheckFailed(f"TLS failed ({e})")
        log(f"  [PASS] TLS ok ({sock.version()})")

    if not use_ws:
        return Plain(sock)

    log("3) WebSocket")
    key = b64encode(os.urandom(16)).decode()
    sock.sendall((f"GET {path} HTTP/1.1\r\nHost: {host}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                  f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n"
                  "Sec-WebSocket-Protocol: mqtt\r\n\r\n").encode())
    reply = b""
    try:
        while b"\r\n\r\n" not in reply:
            chunk = sock.recv(1024)
            if not chunk:
                break
            reply += chunk
    except socket.timeout:
        pass
    status = reply.split(b"\r\n", 1)[0].decode(errors="replace")
    if " 101 " not in status + " ":
        raise CheckFailed(f"the server did not switch to WebSocket at {path} (answer: {status or 'nothing'})"
                          " - wrong path, or this port is not a WebSocket port")
    log(f"  [PASS] {status}")
    return WebSocket(sock)


def mqtt_connect(conn, client_id, user="", password=""):
    """CONNECT: MQTT 3.1.1, clean session, keepalive 30 s, with a login when user is given"""
    flags = 0x02
    payload = mqtt_str(client_id)
    if user:
        flags |= 0xC0
        payload += mqtt_str(user) + mqtt_str(password)
    conn.send(packet(0x10, mqtt_str("MQTT") + bytes([4, flags]) + struct.pack("!H", 30) + payload))
    try:
        ptype, data = read_packet(conn)
    except (socket.timeout, ConnectionError) as e:
        raise CheckFailed(f"the port is open but does not answer like MQTT ({e})"
                          " - this may not be an MQTT port (for 443 try wss://)")
    if ptype != 2:
        raise CheckFailed(f"got packet type {ptype} instead of CONNACK - this port is not MQTT")
    if data[1] != 0:
        raise CheckFailed(f"the broker refused the login: {CONNACK_CODES.get(data[1], data[1])}")


def subscribe(conn, topic):
    conn.send(packet(0x82, struct.pack("!H", 1) + mqtt_str(topic) + b"\x00"))
    ptype, data = read_packet(conn)
    if ptype != 9 or data[2] == 0x80:
        raise CheckFailed(f"SUBSCRIBE {topic} refused - this account may not be allowed on this topic")


def ask_login(user, password):
    """Fill in what was left out on the command line by asking (only when someone is typing)."""
    if user is None:
        user = input("MQTT username (Enter for none): ").strip() if sys.stdin.isatty() else ""
    if user and password is None:
        password = getpass.getpass(f"password for {user}: ")
    return user, password or ""


def round_trip(url, user, password, topic):
    conn = open_transport(url)
    print(f"{4 if url.startswith(('ws', 'wss')) else 3}) MQTT")
    client_id = "cenextgen-check-" + os.urandom(3).hex()
    mqtt_connect(conn, client_id, user, password)
    print(f"  [PASS] logged in" + (f" as {user}" if user else " without a username") + f" (client id {client_id})")

    subscribe(conn, topic)
    print(f"  [PASS] SUBSCRIBE {topic}")

    message = f"hello {client_id}"
    conn.send(packet(0x30, mqtt_str(topic) + message.encode("utf-8")))
    print(f'  [PASS] PUBLISH "{message}"')

    deadline = time.time() + TIMEOUT
    while time.time() < deadline:
        try:
            ptype, data = read_packet(conn)
        except socket.timeout:
            break
        if ptype == 3:
            tlen = struct.unpack("!H", data[:2])[0]
            if data[2 + tlen:].decode("utf-8", "replace") == message:
                print(f"  [PASS] the message came back from the broker"
                      f" ({(time.time() - (deadline - TIMEOUT)) * 1000:.0f} ms)")
                conn.send(b"\xe0\x00")  # DISCONNECT
                return
    raise CheckFailed(f"published but nothing came back within {TIMEOUT} s - this account may not be allowed to publish")


def main():
    sys.stdout.reconfigure(encoding="utf-8")  # Thai payloads print fine even on a non-UTF-8 Windows console
    ap = argparse.ArgumentParser(description="Check the MQTT broker step by step.")
    ap.add_argument("url", nargs="?", default=DEFAULT_URL, help=f"broker url (default {DEFAULT_URL})")
    ap.add_argument("-u", "--user", help="MQTT username")
    ap.add_argument("-p", "--password", help="MQTT password (asked when it runs if left out)")
    ap.add_argument("-t", "--topic", default="CENEXTGEN3/test", help="topic for the test (default CENEXTGEN3/test)")
    args = ap.parse_args()

    user, password = ask_login(args.user, args.password)
    print(f"\nChecking {args.url}  user={user or '(none)'}  topic={args.topic}\n")
    try:
        round_trip(args.url, user, password, args.topic)
    except CheckFailed as e:
        print(f"  [FAIL] {e}")
        sys.exit(1)
    print(f"\nResult: {args.url} works" + (f" with the account {user}" if user else ""))


if __name__ == "__main__":
    main()
