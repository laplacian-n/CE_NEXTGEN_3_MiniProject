// device_check.ino - checks every part of the mini project one by one
// Open Serial Monitor at 115200 baud, then type a letter and press Enter:
//   a = run all tests     r = RFID     s = servo      u = ultrasonic
//   l = LED               b = buzzer   w = WiFi + MQTT broker
// Uses the same pins as miniproject.ino
#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

#define SS_PIN 21
#define SCK_PIN 19
#define MOSI_PIN 18
#define MISO_PIN 5
#define RST_PIN 16
#define TRIG_PIN 4
#define ECHO_PIN 2
#define ledPin 27
#define buzzerPin 13
const int ServoPin = 33;

const char* ssid = "CE_NEXTGEN#4_MCI_2.4GHz";
const char* password = "cenextgenMCI809";
const char* mqtt_broker = "mqtt.ce-esl.com";
const int   mqtt_port = 1883;
const char* mqtt_user = "";   // same username / password as in miniproject.ino
const char* mqtt_pass = "";

MFRC522 rfid(SS_PIN, RST_PIN);
Servo myservo;
WiFiClient espClient;
PubSubClient client(espClient);

int buzzerOff = HIGH;  // the project buzzer is active low (LOW = on); changes to LOW if the test finds an active-high one

enum Result { R_PASS, R_FAIL, R_WARN, R_LOOK, R_SKIP };
const char* resultName[] = {"PASS", "FAIL", "WARN", "LOOK", "SKIP"};

void line() { Serial.println("------------------------------------------------"); }

char waitKey(unsigned long ms)
{
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c != '\n' && c != '\r' && c != ' ') return tolower(c);
    }
    delay(10);
  }
  return 0;
}

// ---------------------------------------------------------------- RFID
Result testRFID()
{
  line();
  Serial.println("[RFID] RC522 module");
  Serial.printf("  pins: SDA=%d SCK=%d MOSI=%d MISO=%d RST=%d, power 3V3\n", SS_PIN, SCK_PIN, MOSI_PIN, MISO_PIN, RST_PIN);
  rfid.PCD_Init();
  delay(50);

  byte v = 0;
  int good = 0;
  for (int i = 0; i < 5; i++) {
    v = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    if (v == 0x88 || v == 0x90 || v == 0x91 || v == 0x92 || v == 0x12 || v == 0xB2) good++;
    delay(20);
  }
  Serial.printf("  chip version: 0x%02X (%d/5 reads OK)\n", v, good);
  if (good == 0) {
    if (v == 0x00) Serial.println("  FAIL: no answer. Check MISO (D5), SDA (D21), 3.3V and GND.");
    else if (v == 0xFF) Serial.println("  FAIL: MISO is floating. Check MISO (D5) and RST (D16).");
    else Serial.println("  FAIL: strange value. SCK/MOSI/MISO may be swapped.");
    return R_FAIL;
  }
  if (good < 5) {
    Serial.println("  FAIL: sometimes right, sometimes wrong. A wire is loose.");
    return R_FAIL;
  }

  byte original = rfid.PCD_ReadRegister(MFRC522::ModWidthReg);
  rfid.PCD_WriteRegister(MFRC522::ModWidthReg, 0x55);
  byte back = rfid.PCD_ReadRegister(MFRC522::ModWidthReg);
  rfid.PCD_WriteRegister(MFRC522::ModWidthReg, original);
  if (back != 0x55) {
    Serial.println("  FAIL: can read the chip but cannot write to it. Check MOSI (D18).");
    return R_FAIL;
  }
  Serial.println("  wiring OK");

  Serial.println("  Tap a card on the reader now (10 seconds)...");
  unsigned long start = millis();
  while (millis() - start < 10000) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      Serial.print("  card UID: ");
      for (byte i = 0; i < rfid.uid.size; i++) Serial.printf("%02X", rfid.uid.uidByte[i]);
      Serial.println();
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
      return R_PASS;
    }
    delay(50);
  }
  Serial.println("  WARN: wiring is OK but no card was read. Hold the card flat on the reader.");
  return R_WARN;
}

// ---------------------------------------------------------------- Servo
Result testServo()
{
  line();
  Serial.printf("[SERVO] signal on D%d, power from VIN (5V)\n", ServoPin);
  const int angles[] = {0, 90, 180, 0};
  for (int a : angles) {
    Serial.printf("  moving to %d degrees\n", a);
    myservo.write(a);
    delay(800);
  }
  Serial.println("  LOOK: the arm should have moved 0 -> 90 -> 180 -> 0.");
  Serial.println("        If it only shakes, check that + goes to VIN, not 3V3.");
  return R_LOOK;
}

// ---------------------------------------------------------------- Ultrasonic
float readDistance()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long us = pulseIn(ECHO_PIN, HIGH, 30000);
  return us == 0 ? -1 : 0.017 * us;
}

Result testUltrasonic()
{
  line();
  Serial.printf("[ULTRASONIC] HC-SR04 Trig=D%d Echo=D%d\n", TRIG_PIN, ECHO_PIN);
  Serial.println("  Move your hand in front of the sensor. 10 readings:");
  int ok = 0;
  float lo = 9999, hi = 0;
  for (int i = 0; i < 10; i++) {
    float d = readDistance();
    if (d < 0) {
      Serial.println("  no echo");
    } else {
      Serial.printf("  %.1f cm\n", d);
      ok++;
      lo = min(lo, d);
      hi = max(hi, d);
    }
    delay(300);
  }
  if (ok == 0) {
    Serial.println("  FAIL: no echo at all. Check Trig (D4), Echo (D2), VCC and GND.");
    return R_FAIL;
  }
  Serial.printf("  %d/10 readings, %.1f - %.1f cm\n", ok, lo, hi);
  if (ok < 7) {
    Serial.println("  WARN: many missed readings. Nothing in front of it, or a loose wire.");
    return R_WARN;
  }
  return R_PASS;
}

// ---------------------------------------------------------------- LED
Result testLED()
{
  line();
  Serial.printf("[LED] D%d through a resistor\n", ledPin);
  for (int i = 0; i < 3; i++) {
    digitalWrite(ledPin, HIGH);
    delay(400);
    digitalWrite(ledPin, LOW);
    delay(400);
  }
  Serial.println("  LOOK: the LED should have blinked 3 times.");
  Serial.println("        If not, turn the LED around (long leg goes to the resistor).");
  return R_LOOK;
}

// ---------------------------------------------------------------- Buzzer
Result testBuzzer()
{
  line();
  Serial.printf("[BUZZER] I/O on D%d\n", buzzerPin);
  Serial.println("  Listen. Part A (pin HIGH) for 1 second...");
  digitalWrite(buzzerPin, HIGH);
  delay(1000);
  Serial.println("  Part B (pin LOW) for 1 second...");
  digitalWrite(buzzerPin, LOW);
  delay(1000);
  digitalWrite(buzzerPin, buzzerOff);

  Serial.println("  Which part beeped? Type A, B or N (none) - 15 seconds");
  char c = waitKey(15000);
  if (c == 'a') {
    buzzerOff = LOW;
    digitalWrite(buzzerPin, buzzerOff);
    Serial.println("  WARN: active-HIGH buzzer, but the project code expects active LOW. Swap HIGH/LOW for the buzzer.");
    return R_WARN;
  }
  if (c == 'b') {
    buzzerOff = HIGH;
    digitalWrite(buzzerPin, buzzerOff);
    Serial.println("  PASS: active-LOW buzzer (LOW = on, HIGH = off), as the project code expects.");
    return R_PASS;
  }
  if (c == 'n') {
    Serial.println("  FAIL: no sound. Check I/O (D13), VCC and GND.");
    return R_FAIL;
  }
  Serial.println("  SKIP: no answer.");
  return R_SKIP;
}

// ---------------------------------------------------------------- WiFi + MQTT
Result testNetwork()
{
  line();
  Serial.printf("[NETWORK] WiFi \"%s\" then MQTT %s:%d\n", ssid, mqtt_broker, mqtt_port);
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(250);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("  FAIL: cannot join WiFi. Check the name/password, or move closer to the router.");
    return R_FAIL;
  }
  Serial.print("  WiFi OK, IP ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_broker, mqtt_port);
  String id = "esp32-debug-" + WiFi.macAddress();
  if (strlen(mqtt_user) == 0) Serial.println("  note: mqtt_user is empty, the broker will probably refuse");
  if (!client.connect(id.c_str(), mqtt_user, mqtt_pass)) {
    int st = client.state();
    if (st == 4) Serial.println("  FAIL: wrong mqtt_user / mqtt_pass (letter case matters).");
    else if (st == 5) Serial.println("  FAIL: this mqtt_user is not allowed on the broker.");
    else Serial.printf("  FAIL: WiFi works but the broker did not answer (state %d). The network may block port 1883.\n", st);
    return R_FAIL;
  }
  client.publish("CENEXTGEN3/debug", "hello from debug.ino");
  client.disconnect();
  Serial.println("  MQTT OK");
  return R_PASS;
}

// ---------------------------------------------------------------- menu
void menu()
{
  line();
  Serial.println("Type a letter and press Enter:");
  Serial.println("  a = run all tests");
  Serial.println("  r = RFID   s = servo   u = ultrasonic");
  Serial.println("  l = LED    b = buzzer  w = WiFi + MQTT");
  Serial.println("(Tapping a card at any time prints its UID)");
}

void runAll()
{
  const char* names[] = {"RFID", "Servo", "Ultrasonic", "LED", "Buzzer", "WiFi + MQTT"};
  Result r[6];
  r[0] = testRFID();
  r[1] = testServo();
  r[2] = testUltrasonic();
  r[3] = testLED();
  r[4] = testBuzzer();
  r[5] = testNetwork();
  line();
  Serial.println("SUMMARY");
  for (int i = 0; i < 6; i++) Serial.printf("  %-12s %s\n", names[i], resultName[r[i]]);
  Serial.println("  PASS = works, FAIL = fix the wiring, WARN = works but unreliable,");
  Serial.println("  LOOK = check with your eyes, SKIP = not answered");
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, buzzerOff);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();
  myservo.setPeriodHertz(50);
  myservo.attach(ServoPin, 1000, 2000);
  myservo.write(0);

  Serial.println();
  Serial.println("===== CE NEXT GEN #3 - DEVICE CHECK =====");
  menu();
}

void loop()
{
  char c = waitKey(50);
  switch (c) {
    case 'a': runAll(); menu(); break;
    case 'r': testRFID(); menu(); break;
    case 's': testServo(); menu(); break;
    case 'u': testUltrasonic(); menu(); break;
    case 'l': testLED(); menu(); break;
    case 'b': testBuzzer(); menu(); break;
    case 'w': testNetwork(); menu(); break;
    case 0: break;
    default: Serial.printf("Unknown key '%c'\n", c); menu();
  }

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    Serial.print("Card UID: ");
    for (byte i = 0; i < rfid.uid.size; i++) Serial.printf("%02X", rfid.uid.uidByte[i]);
    Serial.println();
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}
