"""ดักฟังข้อความใน MQTT topic (ใช้แทนแท็บ debug ของ Node-RED ตอนเช็คว่าบอร์ดส่งข้อความออกมาจริงไหม)

ใช้:  python listen_topic.py [topic] [วินาที] [host] [port]
ค่าเริ่มต้น: CENEXTGEN3/teacher/#  ฟัง 60 วินาที  broker.emqx.io 1883
(# = ทุก topic ที่อยู่ใต้ชื่อนี้)
"""
import os
import socket
import struct
import sys
import time

from test_broker import mqtt_str, packet, read_packet

TOPIC = sys.argv[1] if len(sys.argv) > 1 else "CENEXTGEN3/teacher/#"
SECONDS = int(sys.argv[2]) if len(sys.argv) > 2 else 60
HOST = sys.argv[3] if len(sys.argv) > 3 else "broker.emqx.io"
PORT = int(sys.argv[4]) if len(sys.argv) > 4 else 1883


def main():
    sock = socket.create_connection((HOST, PORT), timeout=5)
    client_id = "cenextgen-listen-" + os.urandom(3).hex()
    body = mqtt_str("MQTT") + bytes([4, 0x02]) + struct.pack("!H", 30) + mqtt_str(client_id)
    sock.sendall(packet(0x10, body))
    ptype, data = read_packet(sock)
    if ptype != 2 or data[1] != 0:
        sys.exit("ต่อ broker ไม่สำเร็จ")
    sock.sendall(packet(0x82, struct.pack("!H", 1) + mqtt_str(TOPIC) + b"\x00"))
    read_packet(sock)
    print(f"กำลังฟัง {TOPIC} บน {HOST}:{PORT} เป็นเวลา {SECONDS} วินาที...", flush=True)

    end = time.time() + SECONDS
    last_ping = time.time()
    while time.time() < end:
        if time.time() - last_ping > 20:
            sock.sendall(b"\xc0\x00")  # PINGREQ กัน broker ตัดเพราะเงียบนาน
            last_ping = time.time()
        try:
            ptype, data = read_packet(sock)
        except socket.timeout:
            continue
        if ptype == 3:
            tlen = struct.unpack("!H", data[:2])[0]
            topic = data[2:2 + tlen].decode("utf-8", "replace")
            payload = data[2 + tlen:].decode("utf-8", "replace")
            print(f"{time.strftime('%H:%M:%S')}  {topic}  ->  {payload}", flush=True)
    sock.close()
    print("หมดเวลา")


if __name__ == "__main__":
    main()
