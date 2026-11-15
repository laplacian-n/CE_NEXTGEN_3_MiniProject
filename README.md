# CE NEXT GEN #3 - Mini Project: ระบบไม้กั้นที่จอดรถอัจฉริยะ

แตะบัตร RFID แล้วไม้กั้น (Servo) เปิด, เซนเซอร์อัลตราโซนิกเตือนเมื่อรถเข้าใกล้, ส่งข้อมูลทั้งหมดผ่าน MQTT ไปแสดงบน Node-RED Dashboard

ดัดแปลงจาก [17punchisama/CE_NEXTGEN_3](https://github.com/17punchisama/CE_NEXTGEN_3) โดย
- **ย้ายขา RC522** ให้เรียงตามลำดับขาบนบอร์ด ESP32 สายใน schematic จะไม่ตัดกัน (ดูตารางด้านล่าง)
- **ย้าย LED ไป D27 และ Echo ไป D2** ให้สายทุกเส้นเป็นเส้นตรง และเลิกใช้ D12 ซึ่งเป็นขา strapping
- **ใส่ `return 999;` ชั่วคราวใน `measureDistance()`** ฟังก์ชันที่ไม่มี return ทำให้บอร์ดทำงานเพี้ยน (servo กระตุก, ส่ง UID ว่างออกมาเรื่อยๆ) ตั้งแต่ Session 1

## ไฟล์ใน repo

| ไฟล์ | ใช้ทำอะไร |
|---|---|
| `miniproject/miniproject.ino` | โค้ดตั้งต้น ใส่ขา, WiFi, broker ไว้ให้แล้ว เหลือตั้งชื่อกลุ่มกับใส่ username/password ของ MQTT (TODO 1) และ TODO 2–10 |
| `debug/` | เครื่องมือหาปัญหา แยกเป็น `hardware/` (โค้ดเช็คอุปกรณ์บนบอร์ด), `network/` (เช็คการต่อ MQTT broker), `nodered/` (เช็ค/ติดตั้ง Node-RED) ดูวิธีใช้ใน [debug/README.md](debug/README.md) ผลเป็นภาษาอังกฤษ |
| `flows.json` | Flow ของ Node-RED ต่อสายและตั้งค่า dashboard ไว้แล้ว เหลือเลือก Server, แก้ชื่อกลุ่ม และวางโค้ดใน function |
| `solution/` | เฉลย |
| `slides/` | สไลด์ประกอบการสอน (PDF) เปิดดูบน GitHub ได้เลย |

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

> buzzer เป็นแบบ active low: **LOW = ดัง, HIGH = เงียบ** (ทั้งโค้ดใช้แบบนี้)

## เริ่มต้น

1. เปิด `miniproject/miniproject.ino` แล้วแก้ **TODO 1**: `#define GROUP_NAME "groupname"` เป็นชื่อกลุ่ม (ภาษาอังกฤษ ไม่มีเว้นวรรค ห้ามซ้ำกลุ่มอื่น เพราะทุกกลุ่มใช้ broker ตัวเดียวกัน) และใส่ `mqtt_user` / `mqtt_pass` ที่ได้รับ (บอร์ดต่อ `mqtt.ce-esl.com` พอร์ต 1883)
2. ใน Node-RED เมนู ☰ → Import → เลือก `flows.json` แล้วแก้ `*ชื่อกลุ่มของท่านผู้ประเสริฐ*` ในกล่อง mqtt สีม่วงทั้ง 3 กล่องเป็นชื่อเดียวกัน และเลือก Server ของ broker (สร้างครั้งแรกด้วยปุ่ม ✏️/+ : Server `mqtt.ce-esl.com` Port `1883` แล้วใส่ Username / Password ชุดเดียวกับบอร์ดในแท็บ Security) → Deploy
   (หรือเปิด `flows.json` ด้วย Notepad กด Ctrl+H แทนที่ `*ชื่อกลุ่มของท่านผู้ประเสริฐ*` ทีเดียวก่อน Import)
3. ดับเบิลคลิกกล่อง **UID to name** แล้วแทน `XXXXXXXX` ด้วย UID ของบัตรที่มีสิทธิ์เข้า (ตัวพิมพ์ใหญ่ ตามที่ขึ้นใน Serial Monitor) เพิ่มได้บรรทัดละใบ บัตรที่มีสิทธิ์จะแสดงชื่อและส่ง `open` ไปที่ topic servo ให้บอร์ดเปิดประตู (TODO 5) บัตรอื่นจะขึ้น "ไม่มีสิทธิ์" และไม่เปิด

ถ้ามีอะไรไม่ทำงาน ดูตารางใน [debug/README.md](debug/README.md) ว่าใช้เครื่องมือตัวไหน เช่น

- อุปกรณ์บนบอร์ดไม่ทำงาน: อัปโหลด `debug/hardware/device_check/device_check.ino` เปิด Serial Monitor ที่ 115200 พิมพ์ `a` แล้ว Enter
- ต่อ MQTT ไม่ได้: Windows ดับเบิลคลิก `debug/network/check_mqtt.cmd`, Linux / macOS รัน `bash debug/network/check_mqtt.sh -u ชื่อผู้ใช้`
- Node-RED ไม่ได้ติดตั้ง หรือรันไม่ได้ หาคำสั่งไม่เจอ: Windows ดับเบิลคลิก `debug/nodered/check_nodered.cmd`, Linux / macOS รัน `bash debug/nodered/check_nodered.sh`
