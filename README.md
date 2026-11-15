# CE NEXT GEN #3 — Mini Project: ระบบไม้กั้นที่จอดรถอัจฉริยะ

แตะบัตร RFID แล้วไม้กั้น (Servo) เปิด, เซนเซอร์อัลตราโซนิกเตือนเมื่อรถเข้าใกล้, ส่งข้อมูลทั้งหมดผ่าน MQTT ไปแสดงบน Node-RED Dashboard

ดัดแปลงจาก [17punchisama/CE_NEXTGEN_3](https://github.com/17punchisama/CE_NEXTGEN_3) โดย
- **ย้ายขา RC522** ให้เรียงตามลำดับขาบนบอร์ด ESP32 สายใน schematic จะไม่ตัดกัน (ดูตารางด้านล่าง)
- **ย้าย LED ไป D27 และ Echo ไป D2** ให้สายทุกเส้นเป็นเส้นตรง และเลิกใช้ D12 ซึ่งเป็นขา strapping
- **ใส่ `return 999;` ชั่วคราวใน `measureDistance()`** ฟังก์ชันที่ไม่มี return ทำให้บอร์ดทำงานเพี้ยน (servo กระตุก, ส่ง UID ว่างออกมาเรื่อยๆ) ตั้งแต่ Session 1

## ไฟล์ใน repo

| ไฟล์ | ใช้ทำอะไร |
|---|---|
| `miniproject/miniproject.ino` | โค้ดตั้งต้น มี TODO 1–10 ให้เติม |
| `ultrasonics/ultrasonics.ino` | โค้ดทดลองวัดระยะด้วย HC-SR04 อย่างเดียว |
| `flows.json` | Flow ตั้งต้นของ Node-RED (Import เข้าไปแล้วเติมเอง) |
| `solution/` | เฉลย |
| `tools/test_broker.py` | เช็คว่าเน็ตต่อ MQTT broker พอร์ต 1883 ได้ไหม |
| `tools/listen_topic.py` | ดักฟังข้อความใน topic ดูว่าบอร์ดส่งข้อความออกมาจริงไหม |
| `tools/rfid_check/` | เช็คเครื่องอ่าน RFID ทีละขั้น บอกว่าต้องแก้สายเส้นไหน |

## อุปกรณ์และ Library

ESP32 DevKit V1 (30 ขา), RFID RC522 + บัตร, Servo SG90, HC-SR04, โมดูล Buzzer, LED + ตัวต้านทาน 220Ω

Arduino IDE: เลือกบอร์ด **ESP32 Dev Module** แล้วติดตั้ง library **PubSubClient**, **MFRC522**, **ESP32Servo**

## การต่อสาย

### RFID RC522

| RC522 | ESP32 |
|---|---|
| SDA | **D21** |
| SCK | **D19** |
| MOSI | **D18** |
| MISO | **D5** |
| IRQ | ไม่ต่อ |
| GND | GND |
| RST | **RX2 (D16)** |
| 3.3V | **3V3** (ห้ามต่อ VIN) |

### อุปกรณ์อื่น

| อุปกรณ์ | ขาอุปกรณ์ | ESP32 |
|---|---|---|
| Servo SG90 | ส้ม (SIG) | D33 |
| | แดง (VCC) | VIN (5V) |
| | น้ำตาล (GND) | GND |
| HC-SR04 | VCC | 3V3 |
| | Trig | D4 |
| | Echo | D2 (ไฟสีฟ้าบนบอร์ดจะกะพริบตอนวัดระยะ) |
| | GND | GND |
| Buzzer | I/O | D13 |
| | VCC / GND | 3V3 / GND |
| LED | ขายาว (A) | D27 ผ่านตัวต้านทาน 220Ω |
| | ขาสั้น (K) | GND |

> โมดูล buzzer บางรุ่นเป็นแบบ **LOW = ดัง** ถ้าดังค้างตั้งแต่เปิดเครื่อง ให้สลับ HIGH/LOW ในโค้ด

## เริ่มต้น

1. เปิด `miniproject/miniproject.ino` แล้วเติม **TODO 1**: ชื่อ WiFi, รหัส WiFi, broker `broker.emqx.io` พอร์ต `1883`
2. ตั้งชื่อ topic ให้ **ไม่ซ้ำกับกลุ่มอื่น** เพราะ broker เป็นสาธารณะ ใช้ร่วมกันทุกกลุ่ม เช่น `CENEXTGEN3/group1/rfid`
3. ใน Node-RED เมนู ☰ → Import → เลือก `flows.json` แล้วตั้ง broker กับ topic ให้ตรงกับโค้ด

ถ้าสงสัยว่าเน็ตในห้องบล็อก MQTT ไหม ให้รัน:

```bash
python tools/test_broker.py broker.emqx.io 1883
```
