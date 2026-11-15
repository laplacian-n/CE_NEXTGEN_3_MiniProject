"""ทดสอบ MQTT broker ว่าใช้งานได้จริงที่ host/port ที่กำหนด — ไม่ต้องลง library เพิ่ม

ใช้:  python test_broker.py [host] [port] [topic]
ค่าเริ่มต้น: broker.emqx.io 1883 CENEXTGEN3/teacher/test

ทดสอบ 3 ขั้น:
  1. DNS   — หา IP ของ host ได้ไหม
  2. TCP   — เปิดพอร์ตได้ไหม (เน็ต/ไฟร์วอลล์บล็อกหรือเปล่า)
  3. MQTT  — CONNECT แล้ว subscribe + publish ข้อความเข้า topic ตัวเอง แล้วรอรับกลับ
"""
import os
import socket
import struct
import sys
import time

HOST = sys.argv[1] if len(sys.argv) > 1 else "broker.emqx.io"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 1883
TOPIC = sys.argv[3] if len(sys.argv) > 3 else "CENEXTGEN3/teacher/test"
TIMEOUT = 5

CONNACK_CODES = {
    0: "accepted",
    1: "unacceptable protocol version",
    2: "client id rejected",
    3: "server unavailable",
    4: "bad username/password",
    5: "not authorized",
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


def recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("broker ปิดการเชื่อมต่อ")
        buf += chunk
    return buf


def read_packet(sock):
    header = recv_exact(sock, 1)[0]
    mult, length = 1, 0
    while True:
        b = recv_exact(sock, 1)[0]
        length += (b & 0x7F) * mult
        if not b & 0x80:
            break
        mult *= 128
    return header >> 4, recv_exact(sock, length)


def ok(msg):
    print(f"  [ผ่าน] {msg}")


def fail(msg):
    print(f"  [ไม่ผ่าน] {msg}")
    sys.exit(1)


def main():
    print(f"ทดสอบ {HOST}:{PORT}  topic={TOPIC}\n")

    print("1) DNS")
    try:
        ip = socket.gethostbyname(HOST)
    except socket.gaierror as e:
        fail(f"หา IP ของ {HOST} ไม่ได้ ({e}) — เช็คอินเทอร์เน็ต/สะกดชื่อ")
    ok(f"{HOST} -> {ip}")

    print("2) TCP")
    t0 = time.time()
    try:
        sock = socket.create_connection((HOST, PORT), timeout=TIMEOUT)
    except OSError as e:
        fail(f"เปิดพอร์ต {PORT} ไม่ได้ ({e}) — พอร์ตผิด หรือเน็ตบล็อกพอร์ตนี้")
    ok(f"เปิดพอร์ต {PORT} ได้ ({(time.time() - t0) * 1000:.0f} ms)")

    print("3) MQTT")
    sock.settimeout(TIMEOUT)
    client_id = "cenextgen-test-" + os.urandom(3).hex()
    # CONNECT: MQTT 3.1.1, clean session, keepalive 30s
    body = mqtt_str("MQTT") + bytes([4, 0x02]) + struct.pack("!H", 30) + mqtt_str(client_id)
    sock.sendall(packet(0x10, body))
    try:
        ptype, data = read_packet(sock)
    except (socket.timeout, ConnectionError) as e:
        fail(f"พอร์ตเปิดแต่ไม่ตอบแบบ MQTT ({e}) — พอร์ตนี้อาจไม่ใช่ MQTT")
    if ptype != 2:
        fail(f"ได้แพ็กเก็ตชนิด {ptype} แทน CONNACK — พอร์ตนี้ไม่ใช่ MQTT")
    if data[1] != 0:
        fail(f"broker ปฏิเสธ: {CONNACK_CODES.get(data[1], data[1])}")
    ok(f"CONNECT สำเร็จ (client id {client_id})")

    sock.sendall(packet(0x82, struct.pack("!H", 1) + mqtt_str(TOPIC) + b"\x00"))
    ptype, data = read_packet(sock)
    if ptype != 9 or data[2] == 0x80:
        fail("SUBSCRIBE ไม่สำเร็จ")
    ok(f"SUBSCRIBE {TOPIC}")

    message = f"hello {client_id}"
    sock.sendall(packet(0x30, mqtt_str(TOPIC) + message.encode("utf-8")))
    ok(f'PUBLISH "{message}"')

    deadline = time.time() + TIMEOUT
    while time.time() < deadline:
        try:
            ptype, data = read_packet(sock)
        except socket.timeout:
            break
        if ptype == 3:
            tlen = struct.unpack("!H", data[:2])[0]
            payload = data[2 + tlen:].decode("utf-8", "replace")
            if payload == message:
                ok(f"ได้ข้อความกลับมาจาก broker ({(time.time() - (deadline - TIMEOUT)) * 1000:.0f} ms)")
                sock.sendall(b"\xe0\x00")  # DISCONNECT
                sock.close()
                print(f"\nสรุป: {HOST}:{PORT} เป็น MQTT broker ที่ใช้งานได้จริง")
                return
    fail("ส่งไปแล้วแต่ไม่ได้ข้อความกลับมาภายใน 5 วินาที")


if __name__ == "__main__":
    main()
