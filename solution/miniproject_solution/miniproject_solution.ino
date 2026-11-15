// เฉลยสำหรับผู้สอน — อิงจาก miniproject.ino ใน github.com/17punchisama/CE_NEXTGEN_3
// ทุกจุดที่เติมมีป้าย TODO เดิมกำกับไว้
#include <WiFi.h>
#include <PubSubClient.h>
#include <MFRC522.h>
#include <SPI.h>
#include <ESP32Servo.h>

String rfidUID = "";    // เอาไว้รับ UserID ที่สแกนได้จาก RFID

// ขาของ RC522 เรียงตามลำดับขาบนบอร์ด ESP32 เพื่อให้สายไม่ตัดกัน
#define SS_PIN 21       // SDA / SS ของ RC522
#define SCK_PIN 19      // SCK ของ RC522
#define MOSI_PIN 18     // MOSI ของ RC522
#define MISO_PIN 5      // MISO ของ RC522
#define RST_PIN 16      // RST ของ RC522 (ขา RX2 บนบอร์ด)

#define TRIG_PIN 4      // ESP32 pin D4 เชื่อมต่อกับ Trig pin
#define ECHO_PIN 15     // ESP32 pin D15 เชื่อมต่อกับ Echo pin

#define ledPin 12       // Pin ของ  LED แสดงสถานะไฟ
#define buzzerPin 13    // Pin ควบคุมการแสดง Buzzer
#define BUZZER_ON  HIGH // โมดูล buzzer บางรุ่นเป็นแบบ LOW = ดัง ให้สลับค่าสองบรรทัดนี้
#define BUZZER_OFF LOW

float duration_us, distance_cm;

const int ServoPin = 33;
Servo myservo;

MFRC522 rfid(SS_PIN, RST_PIN);  // Create MFRC522 instance

// TODO 1 : ตั้งค่า WIFI & PASSWORD, Topic RFID/Servo/Car
//=================================================================================================
WiFiClient   espClient;                     //คำสั่งจาก library
PubSubClient client(espClient);             //สร้างออปเจ็ค สำหรับเชื่อมต่อ mqtt
//=================================================================================================
const char* ssid = "wifi name";                   //wifi name
const char* password = "wifi password";           //wifi password
//=================================================================================================
const char* mqtt_broker = "broker.emqx.io";       //IP mqtt server
const int   mqtt_port = 1883;                     //port mqtt server
// broker เป็นสาธารณะ ใช้ร่วมกันทุกกลุ่ม -> ชื่อ topic ต้องไม่ซ้ำกลุ่มอื่น
const char* topic_rfid = "CENEXTGEN3/group1/rfid";    //Topic for RFID
const char* topic_servo = "CENEXTGEN3/group1/servo";  //Topic for control servo
const char* topic_car = "CENEXTGEN3/group1/car";      //Topic for sending notification if car was hit
//=================================================================================================
const int DOOR_OPEN_ANGLE = 180;
const int DOOR_CLOSE_ANGLE = 0;

void openDoor()
{
    // TODO 4 : สั่งให้ Servo เปิดประตู
    myservo.write(DOOR_OPEN_ANGLE);
}

void closeDoor()
{
    // TODO 5 : สั่งให้ Servo ปิดประตู
    myservo.write(DOOR_CLOSE_ANGLE);
}

float measureDistance()
{
    // TODO 6 : วัดระยะทาง และ return ค่าออกไป
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    duration_us = pulseIn(ECHO_PIN, HIGH, 30000);   // timeout 30ms (~5 m)
    if (duration_us == 0) return 999;               // ไม่มีเสียงสะท้อน = ไกลมาก (กันไม่ให้ตีความเป็น "ชน")

    distance_cm = 0.017 * duration_us;
    return distance_cm;
}

void setup_wifi() {   //ฟังก์ชั่นเชื่อมต่อwifi
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);                      //เลือกโหมดรับ wifi
  WiFi.begin(ssid, password);               //เชื่อมต่อ wifi
  while (WiFi.status() != WL_CONNECTED)     //รอจนกว่าจะเชื่อมต่อwifiสำเร็จ
  {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length) {  //ฟังก์ชั่นsubscribe
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");

  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  Serial.println("-----------------------");

  if (strcmp(topic, topic_servo) == 0) {
    // TODO 3.5 : ถ้าข้อความที่ได้เป็นคำว่า "open" เปิดประตู 2 วินาที และปิด
    if (message == "open") {
      openDoor();
      delay(2000);
      closeDoor();
    }
  }
}

void reconnect() {  //ฟังก์ชั่นเชื่อมต่อmqtt
  client.setServer(mqtt_broker, mqtt_port);   //เชื่อมต่อmqtt
  client.setCallback(callback);               //เลือกฟังก์ชั่นsubscribe
  while (!client.connected())                 //รอจนกว่าจะเชื่อมต่อmqttสำเร็จ
  {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if (client.connect(client_id.c_str()))
      Serial.println("Public emqx mqtt broker connected");
    else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  setup_wifi(); //เชื่อมต่อwifi
  reconnect();  //เชื่อมต่อmqtt
  client.subscribe(topic_servo);  //กำหนด topic ที่จะ subscribe
  // TODO 1.5 : ใส่ชื่อกลุ่ม
  client.publish(topic_rfid, "กรุณาสแกน RFID เพื่อยืนยันตัวตน (CE-NEXT-GEN #3)"); //กำหนด topic ที่จะ publish

  // SPI ของ RC522
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();

  // Set Up ของ Servo
  myservo.setPeriodHertz(50);
  myservo.attach(ServoPin, 1000, 2000);

  // PinMode ของ Ultrasonics
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  // Buzzer
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, BUZZER_OFF);
}

void loop()
{
  client.loop(); //วนลูปรอsubscribe valu จาก mqtt

  if (rfid.PICC_IsNewCardPresent()) {
    Serial.println("Card present!");

    if (!rfid.PICC_ReadCardSerial()) {
      Serial.println("Card detected but read failed");
    } else {

      // สร้าง UID จากบัตรที่อ่านได้
      String scannedUID = "";
      Serial.print("UID tag : ");
      for (byte i = 0; i < rfid.uid.size; i++) {
        scannedUID += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
        scannedUID += String(rfid.uid.uidByte[i], HEX);
      }
      scannedUID.toUpperCase();
      Serial.println(scannedUID);

      // TODO 2 : ส่งรหัส RFID ที่สแกนได้ไปยัง Topic 'topic_rfid'
      client.publish(topic_rfid, scannedUID.c_str());

      // TODO 3 : เปิดประตู 2 วินาที และปิด
      openDoor();
      delay(2000);
      closeDoor();

      // หยุดการ์ดใบนี้
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
  }

  float distance = measureDistance();
  Serial.print("Distance: ");
  Serial.println(distance);
  if (distance > 20){
    // TODO 7 : ส่งข้อความว่า nothing ไปที่ topic_car และสั่ง logic high ให้ LED
    client.publish(topic_car, "nothing");
    digitalWrite(ledPin, HIGH);
  } else if (distance <= 20 && distance > 10) {
    // TODO 8 : ส่งข้อความว่า nothing ไปที่ topic_car และสั่ง logic low ให้ LED
    client.publish(topic_car, "nothing");
    digitalWrite(ledPin, LOW);
  } else if (distance <= 10 && distance > 5) {
    // TODO 9 : ส่งข้อความว่า nothing ไปที่ topic_car และสั่ง buzzer เล่นสัญญาณเตือน
    client.publish(topic_car, "nothing");
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, BUZZER_ON);
    delay(100);
    digitalWrite(buzzerPin, BUZZER_OFF);
    delay(100);
  } else {
    // TODO 10 : ส่งข้อความว่า hit ไปที่ topic_car
    client.publish(topic_car, "hit");
    digitalWrite(ledPin, LOW);
  }

  delay(100);   // ไม่ให้ยิงข้อความถี่เกินไปจน broker ตัดการเชื่อมต่อ
}
