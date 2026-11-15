// rfid_check.ino - checks the RFID-RC522 reader step by step and says which wire to fix
// Uses the same pins as miniproject.ino. Open Serial Monitor at 115200, then press EN to see it from the start
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 21       // SDA / SS of the RC522
#define RST_PIN 16      // RST of the RC522 (the RX2 pin on the board)
#define SCK_PIN 19
#define MISO_PIN 5
#define MOSI_PIN 18

MFRC522 rfid(SS_PIN, RST_PIN);

bool versionOk(byte v)
{
  return v == 0x88 || v == 0x90 || v == 0x91 || v == 0x92 || v == 0x12 || v == 0xB2;
}

// Step 1: read the chip version (tests SCK, MISO, SDA and power)
bool checkVersion()
{
  Serial.println("[1] Read VersionReg 5 times");
  int good = 0;
  byte last = 0;
  for (int i = 0; i < 5; i++) {
    last = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    Serial.printf("    try %d: 0x%02X\n", i + 1, last);
    if (versionOk(last)) good++;
    delay(50);
  }
  if (good == 5) {
    Serial.println("    PASS: the chip gave a valid version every time");
    return true;
  }
  if (good > 0) {
    Serial.println("    FAIL: sometimes right, sometimes wrong -> loose wire / bad solder joint / wires too long");
  } else if (last == 0x00) {
    Serial.println("    FAIL: got 0x00 -> MISO is not on D5, SDA is not on D21, or the reader has no 3.3V / GND");
  } else if (last == 0xFF) {
    Serial.println("    FAIL: got 0xFF -> MISO is floating (not connected / wrong pin) or RST is not on D16 (RX2)");
  } else {
    Serial.println("    FAIL: odd value -> SCK/MOSI/MISO swapped, or a clone chip the library does not know");
  }
  return false;
}

// Step 2: write a register and read it back (tests MOSI - step 1 can pass with MOSI wrong)
bool checkWriteRead()
{
  Serial.println("[2] Write and read back a register (ModWidthReg)");
  byte original = rfid.PCD_ReadRegister(MFRC522::ModWidthReg);
  const byte patterns[] = {0x55, 0xAA, 0x00, 0xFF};
  bool ok = true;
  for (byte p : patterns) {
    rfid.PCD_WriteRegister(MFRC522::ModWidthReg, p);
    byte got = rfid.PCD_ReadRegister(MFRC522::ModWidthReg);
    Serial.printf("    wrote 0x%02X read 0x%02X %s\n", p, got, got == p ? "ok" : "WRONG");
    if (got != p) ok = false;
  }
  rfid.PCD_WriteRegister(MFRC522::ModWidthReg, original);
  Serial.println(ok ? "    PASS: data reaches the chip"
                    : "    FAIL: reads work but writes do not -> MOSI is not on D18 (or swapped with MISO)");
  return ok;
}

// Step 3: the chip's own self test (tests the circuit inside the module)
bool checkSelfTest()
{
  Serial.println("[3] Chip self test");
  bool ok = rfid.PCD_PerformSelfTest();
  rfid.PCD_Init();   // the self test leaves the chip reset, so init it again
  Serial.println(ok ? "    PASS"
                    : "    FAIL (some clone modules fail this but still read cards: if steps 1-2 passed, just try a card)");
  return ok;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("===== RFID-RC522 CHECK =====");
  Serial.printf("Pins: SDA=D%d SCK=D%d MOSI=D%d MISO=D%d RST=D%d\n", SS_PIN, SCK_PIN, MOSI_PIN, MISO_PIN, RST_PIN);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();
  delay(50);

  bool v = checkVersion();
  bool w = v && checkWriteRead();
  if (w) checkSelfTest();

  Serial.println("============================");
  if (w) {
    Serial.println("Wiring is OK -> tap a card on the reader");
  } else {
    Serial.println("Fix the wiring as shown above, then press EN to check again");
  }
}

void loop()
{
  // Step 4: tap a card - prints everything that is read, including bad reads
  if (!rfid.PICC_IsNewCardPresent()) return;

  if (!rfid.PICC_ReadCardSerial()) {
    Serial.println("[4] card seen but not read -> hold the card still, flat on the reader");
    delay(300);
    return;
  }

  if (rfid.uid.size == 0) {
    Serial.println("[4] empty UID (size 0) -> noise, not a real card");
  } else {
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    MFRC522::PICC_Type type = rfid.PICC_GetType(rfid.uid.sak);
    Serial.printf("[4] card UID=%s  (%d bytes, type %s)\n", uid.c_str(), rfid.uid.size,
                  rfid.PICC_GetTypeName(type));
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(300);
}
