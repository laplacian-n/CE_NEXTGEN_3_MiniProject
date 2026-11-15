"""ทดสอบ MQTT broker ที่ต้องใช้ username/password ได้ทั้งแบบ TCP (mqtt://) และ WebSocket (ws:// wss://) - ไม่ต้องลง library เพิ่ม

ใช้:  python test_mqtt.py <url> <username> [password] [topic]
  url ตัวอย่าง
    mqtt://mqtt.ce-esl.com:1883          แบบเดียวกับที่ ESP32 (PubSubClient) ใช้
    wss://mqtt.ce-esl.com:443/mqtt       แบบเดียวกับ MQTTX / Node-RED ผ่าน WebSocket
  ถ้าไม่ใส่ password จะถามตอนรัน (พิมพ์แล้วไม่ขึ้นบนจอ)
  topic เริ่มต้น: CENEXTGEN3/test

ทดสอบ 4 ขั้น:
  1. DNS        - หา IP ของ host ได้ไหม
  2. TCP / TLS  - เปิดพอร์ตได้ไหม (และ TLS ผ่านไหมถ้าเป็น wss/mqtts)
  3. WebSocket  - ขอเปลี่ยนเป็น WebSocket ที่ path ได้ไหม (เฉพาะ ws/wss)
  4. MQTT       - CONNECT ด้วย username/password แล้ว subscribe + publish เข้า topic ตัวเอง แล้วรอรับกลับ
"""
import getpass
import os
import socket
import ssl
import struct
import sys
import time
from base64 import b64encode
from urllib.parse import urlparse

TIMEOUT = 5
DEFAULT_PORTS = {"mqtt": 1883, "mqtts": 8883, "ws": 80, "wss": 443}

CONNACK_CODES = {
    0: "accepted",
    1: "unacceptable protocol version",
    2: "client id rejected",
    3: "server unavailable",
    4: "bad username/password - username หรือ password ผิด (ตัวพิมพ์เล็ก/ใหญ่มีผล)",
    5: "not authorized - ไม่มีบัญชีนี้ หรือบัญชีนี้ไม่มีสิทธิ์",
}


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
    """ส่ง/รับ byte ตรงๆ บน socket (mqtt:// mqtts://)"""

    def __init__(self, sock):
        self.sock = sock

    def send(self, data):
        self.sock.sendall(data)

    def recv(self, n):
        chunk = self.sock.recv(n)
        if not chunk:
            raise ConnectionError("broker ปิดการเชื่อมต่อ")
        return chunk


class WebSocket:
    """ห่อ MQTT ไว้ใน WebSocket binary frame (ws:// wss://)"""

    def __init__(self, sock):
        self.sock = sock
        self.buf = b""

    def _read(self, n):
        out = b""
        while len(out) < n:
            chunk = self.sock.recv(n - len(out))
            if not chunk:
                raise ConnectionError("broker ปิดการเชื่อมต่อ")
            out += chunk
        return out

    def send(self, data):
        # frame ฝั่ง client ต้อง mask ทุกครั้ง
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
                raise ConnectionError("broker ปิด WebSocket")
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


def ok(msg):
    print(f"  [ผ่าน] {msg}")


def fail(msg):
    print(f"  [ไม่ผ่าน] {msg}")
    sys.exit(1)


def main():
    sys.stdout.reconfigure(encoding="utf-8")  # ให้พิมพ์ภาษาไทยได้แม้ console ของ Windows ไม่ได้ตั้งเป็น UTF-8
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    url = urlparse(sys.argv[1] if "://" in sys.argv[1] else "mqtt://" + sys.argv[1])
    scheme = url.scheme
    if scheme not in DEFAULT_PORTS:
        fail(f"ไม่รู้จัก {scheme}:// ใช้ได้แค่ mqtt mqtts ws wss")
    host, port = url.hostname, url.port or DEFAULT_PORTS[scheme]
    path = url.path or "/mqtt"
    user = sys.argv[2]
    password = sys.argv[3] if len(sys.argv) > 3 else getpass.getpass(f"password ของ {user}: ")
    topic = sys.argv[4] if len(sys.argv) > 4 else "CENEXTGEN3/test"
    use_tls = scheme in ("mqtts", "wss")
    use_ws = scheme in ("ws", "wss")

    where = f"{scheme}://{host}:{port}" + (path if use_ws else "")
    print(f"ทดสอบ {where}  user={user}  topic={topic}\n")

    print("1) DNS")
    try:
        ip = socket.gethostbyname(host)
    except socket.gaierror as e:
        fail(f"หา IP ของ {host} ไม่ได้ ({e}) - เช็คอินเทอร์เน็ต/สะกดชื่อ")
    ok(f"{host} -> {ip}")

    print("2) TCP" + " / TLS" * use_tls)
    t0 = time.time()
    try:
        sock = socket.create_connection((host, port), timeout=TIMEOUT)
    except OSError as e:
        fail(f"เปิดพอร์ต {port} ไม่ได้ ({e}) - พอร์ตผิด หรือเน็ตบล็อกพอร์ตนี้")
    ok(f"เปิดพอร์ต {port} ได้ ({(time.time() - t0) * 1000:.0f} ms)")
    sock.settimeout(TIMEOUT)
    if use_tls:
        try:
            sock = ssl.create_default_context().wrap_socket(sock, server_hostname=host)
        except (ssl.SSLError, OSError) as e:
            fail(f"TLS ไม่ผ่าน ({e})")
        ok(f"TLS ผ่าน ({sock.version()})")

    if use_ws:
        print("3) WebSocket")
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
            fail(f"server ไม่ยอมเปลี่ยนเป็น WebSocket ที่ {path} (ตอบ: {status or 'เงียบ'}) - path ผิด หรือพอร์ตนี้ไม่ใช่ WebSocket")
        ok(f"{status}")
        conn = WebSocket(sock)
    else:
        conn = Plain(sock)

    print(f"{4 if use_ws else 3}) MQTT")
    client_id = "cenextgen-test-" + os.urandom(3).hex()
    # CONNECT: MQTT 3.1.1, clean session + username + password, keepalive 30s
    body = (mqtt_str("MQTT") + bytes([4, 0xC2]) + struct.pack("!H", 30)
            + mqtt_str(client_id) + mqtt_str(user) + mqtt_str(password))
    conn.send(packet(0x10, body))
    try:
        ptype, data = read_packet(conn)
    except (socket.timeout, ConnectionError) as e:
        fail(f"พอร์ตเปิดแต่ไม่ตอบแบบ MQTT ({e}) - พอร์ตนี้อาจไม่ใช่ MQTT (ถ้าเป็น 443 ให้ลอง wss://)")
    if ptype != 2:
        fail(f"ได้แพ็กเก็ตชนิด {ptype} แทน CONNACK - พอร์ตนี้ไม่ใช่ MQTT")
    if data[1] != 0:
        fail(f"broker ปฏิเสธ: {CONNACK_CODES.get(data[1], data[1])}")
    ok(f"CONNECT สำเร็จ (client id {client_id})")

    conn.send(packet(0x82, struct.pack("!H", 1) + mqtt_str(topic) + b"\x00"))
    ptype, data = read_packet(conn)
    if ptype != 9 or data[2] == 0x80:
        fail(f"SUBSCRIBE {topic} ไม่สำเร็จ - บัญชีนี้อาจไม่มีสิทธิ์ใน topic นี้")
    ok(f"SUBSCRIBE {topic}")

    message = f"hello {client_id}"
    conn.send(packet(0x30, mqtt_str(topic) + message.encode("utf-8")))
    ok(f'PUBLISH "{message}"')

    deadline = time.time() + TIMEOUT
    while time.time() < deadline:
        try:
            ptype, data = read_packet(conn)
        except socket.timeout:
            break
        if ptype == 3:
            tlen = struct.unpack("!H", data[:2])[0]
            payload = data[2 + tlen:].decode("utf-8", "replace")
            if payload == message:
                ok(f"ได้ข้อความกลับมาจาก broker ({(time.time() - (deadline - TIMEOUT)) * 1000:.0f} ms)")
                conn.send(b"\xe0\x00")  # DISCONNECT
                sock.close()
                print(f"\nสรุป: {where} ใช้งานได้จริงด้วยบัญชี {user}")
                return
    fail("ส่งไปแล้วแต่ไม่ได้ข้อความกลับมาภายใน 5 วินาที - บัญชีนี้อาจ publish ไม่ได้")


if __name__ == "__main__":
    main()
