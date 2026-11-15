// โค้ดเช็คเครื่องอ่าน RFID-RC522 ทีละขั้น — ใช้ขาเดียวกับ miniproject.ino
// เปิด Serial Monitor ที่ 115200 แล้วกด EN เพื่อดูผลตั้งแต่ต้น
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 21       // SDA / SS ของ RC522
#define RST_PIN 16      // RST ของ RC522 (ขา RX2 บนบอร์ด)
#define SCK_PIN 19
#define MISO_PIN 5
#define MOSI_PIN 18

MFRC522 rfid(SS_PIN, RST_PIN);

bool versionOk(byte v)
{
  return v == 0x88 || v == 0x90 || v == 0x91 || v == 0x92 || v == 0x12 || v == 0xB2;
}

// ขั้น 1: อ่านเลขรุ่นของชิป (ทดสอบ SCK, MISO, SDA, ไฟเลี้ยง)
bool checkVersion()
{
  Serial.println("[1] อ่าน VersionReg 5 ครั้ง");
  int good = 0;
  byte last = 0;
  for (int i = 0; i < 5; i++) {
    last = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    Serial.printf("    ครั้งที่ %d: 0x%02X\n", i + 1, last);
    if (versionOk(last)) good++;
    delay(50);
  }
  if (good == 5) {
    Serial.println("    ผ่าน: ชิปตอบเลขรุ่นถูกต้องทุกครั้ง");
    return true;
  }
  if (good > 0) {
    Serial.println("    ไม่ผ่าน: ตอบถูกบ้างผิดบ้าง -> สายหลวม / ขาบัดกรีไม่แน่น / สายยาวเกินไป");
  } else if (last == 0x00) {
    Serial.println("    ไม่ผ่าน: ได้ 0x00 -> MISO ไม่ได้ต่อที่ D5, SDA ไม่ได้ต่อที่ D21, หรือ RFID ไม่มีไฟ/ไม่มี GND");
  } else if (last == 0xFF) {
    Serial.println("    ไม่ผ่าน: ได้ 0xFF -> MISO ลอย (ไม่ได้ต่อ/ต่อผิดขา) หรือ RST ไม่ได้ต่อที่ D16 (RX2)");
  } else {
    Serial.println("    ไม่ผ่าน: ได้ค่าแปลก -> SCK/MOSI/MISO สลับกัน หรือเป็นโมดูลปลอมที่ library ไม่รู้จัก");
  }
  return false;
}

// ขั้น 2: เขียนค่าลง register แล้วอ่านกลับ (ทดสอบ MOSI — ขั้น 1 ผ่านได้แม้ MOSI ต่อผิด)
bool checkWriteRead()
{
  Serial.println("[2] เขียน-อ่าน register (ModWidthReg)");
  byte original = rfid.PCD_ReadRegister(MFRC522::ModWidthReg);
  const byte patterns[] = {0x55, 0xAA, 0x00, 0xFF};
  bool ok = true;
  for (byte p : patterns) {
    rfid.PCD_WriteRegister(MFRC522::ModWidthReg, p);
    byte got = rfid.PCD_ReadRegister(MFRC522::ModWidthReg);
    Serial.printf("    เขียน 0x%02X อ่านได้ 0x%02X %s\n", p, got, got == p ? "ok" : "ผิด");
    if (got != p) ok = false;
  }
  rfid.PCD_WriteRegister(MFRC522::ModWidthReg, original);
  Serial.println(ok ? "    ผ่าน: ส่งข้อมูลเข้าชิปได้"
                    : "    ไม่ผ่าน: อ่านได้แต่เขียนไม่เข้า -> MOSI ไม่ได้ต่อที่ D18 (หรือสลับกับ MISO)");
  return ok;
}

// ขั้น 3: self test ของชิปเอง (ทดสอบวงจรภายในโมดูล)
bool checkSelfTest()
{
  Serial.println("[3] Self test ของชิป");
  bool ok = rfid.PCD_PerformSelfTest();
  rfid.PCD_Init();   // self test ทิ้งชิปไว้ในสภาพรีเซ็ต ต้อง init ใหม่
  Serial.println(ok ? "    ผ่าน"
                    : "    ไม่ผ่าน: (โมดูลโคลนบางรุ่นไม่ผ่านขั้นนี้แต่ยังอ่านบัตรได้ ถ้าขั้น 1-2 ผ่านให้ข้ามไปลองแตะบัตร)");
  return ok;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("===== RFID-RC522 CHECK =====");
  Serial.printf("ขา: SDA=D%d SCK=D%d MOSI=D%d MISO=D%d RST=D%d\n", SS_PIN, SCK_PIN, MOSI_PIN, MISO_PIN, RST_PIN);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();
  delay(50);

  bool v = checkVersion();
  bool w = v && checkWriteRead();
  if (w) checkSelfTest();

  Serial.println("============================");
  if (w) {
    Serial.println("สายต่อถูกต้อง -> ลองแตะบัตรได้เลย");
  } else {
    Serial.println("แก้สายตามคำแนะนำด้านบน แล้วกด EN เพื่อเช็คใหม่");
  }
}

void loop()
{
  // ขั้น 4: ลองแตะบัตร — แสดงทุกอย่างที่อ่านได้ รวมถึงการอ่านที่ผิดปกติ
  if (!rfid.PICC_IsNewCardPresent()) return;

  if (!rfid.PICC_ReadCardSerial()) {
    Serial.println("[4] เจอบัตรแต่อ่านไม่สำเร็จ -> ถือบัตรให้นิ่ง แนบกับเครื่องอ่าน");
    delay(300);
    return;
  }

  if (rfid.uid.size == 0) {
    Serial.println("[4] อ่านได้ UID ว่าง (ขนาด 0) -> สัญญาณรบกวน ไม่ใช่บัตรจริง");
  } else {
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    MFRC522::PICC_Type type = rfid.PICC_GetType(rfid.uid.sak);
    Serial.printf("[4] บัตร UID=%s  (%d ไบต์, ชนิด %s)\n", uid.c_str(), rfid.uid.size,
                  rfid.PICC_GetTypeName(type));
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(300);
}
