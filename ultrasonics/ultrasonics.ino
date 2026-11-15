#define TRIG_PIN 4 // ESP32 pin D4 เชื่อมต่อกับ Trig pin
#define ECHO_PIN 2 // ESP32 pin D2 เชื่อมต่อกับ Echo pin

float duration_us, distance_cm;

void setup()
{
    // begin serial port
    Serial.begin(9600);

    // configure the trigger pin to output mode
    pinMode(TRIG_PIN, OUTPUT);
    // configure the echo pin to input mode
    pinMode(ECHO_PIN, INPUT);
} 

float measureDistance()
{
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // measure duration of pulse from ECHO pin
    duration_us = pulseIn(ECHO_PIN, HIGH);

    // calculate the distance
    distance_cm = 0.017 * duration_us;

    // print the value to Serial Monitor
    return distance_cm;
}

void loop()
{
    // generate 10-microsecond pulse to TRIG pin
    float distance = measureDistance();
    Serial.print("Distance: ");
    Serial.println(distance);
}
