#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>

// OLED Setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// WiFi Credentials
const char* ssid = "project";
const char* password = "12345678";

// ThingSpeak
String apiKey = "9DEA80M3G1Y4FA46";
const char* server = "http://api.thingspeak.com/update";

// Sensor Pins
#define TDS_PIN   34
#define TURB_PIN  35
#define PH_PIN    32

// Motor Pins
#define MOTOR_IN1 26
#define MOTOR_IN2 27
#define MOTOR_ENA 14

// Thresholds
const float TDS_THRESHOLD  = 3000.0;
const float NTU_THRESHOLD  = 3000.0;
const float PH_LOW_LIMIT   = 0.0;
const float PH_HIGH_LIMIT  = 15.0;

// PWM
const int freq = 5000;
const int resolution = 8;

// Averaging
const int samples = 10;

void setup() {
  Serial.begin(115200);

  // Keep pulldown so pin becomes 0V if sensor removed
  pinMode(TDS_PIN, INPUT_PULLDOWN);
  pinMode(TURB_PIN, INPUT_PULLDOWN);
  pinMode(PH_PIN, INPUT_PULLDOWN);

  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  ledcAttach(MOTOR_ENA, freq, resolution);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    while (1);
  }

  display.clearDisplay();
  display.display();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
}

// Averaging Function
float readAverage(int pin) {
  long total = 0;
  for (int i = 0; i < samples; i++) {
    total += analogRead(pin);
    delay(5);
  }
  return (float)total / samples;
}

void loop() {

  float avgTds = readAverage(TDS_PIN);
  float vTds = (avgTds / 4095.0) * 3.3;

  float avgTurb = readAverage(TURB_PIN);
  float vTurb = (avgTurb / 4095.0) * 3.3;

  float avgPh = readAverage(PH_PIN);
  float vPh = (avgPh / 4095.0) * 3.3;

  // ✅ Correct Sensor Disconnect Detection
  bool tdsMissing  = (vTds < 0.05);
  bool turbMissing = (vTurb < 0.05);
  bool phMissing   = (vPh < 0.05);

  bool sensorError = tdsMissing || turbMissing || phMissing;

  if (sensorError) {
    runMotor(0);

    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(10, 20);
    display.print("SENSOR");
    display.setCursor(10, 40);
    display.print("ERROR");
    display.display();

    Serial.println("Sensor Disconnected!");
    delay(200);
    return;
  }

  float tdsValue = (133.42 * pow(vTds, 3)
                   - 255.86 * pow(vTds, 2)
                   + 857.39 * vTds) * 0.5;

  float ntu = -1120.4 * sq(vTurb)
              + 5742.3 * vTurb
              - 4353.8;

  if (ntu < 0) ntu = 0;

  float phValue = 7 + ((2.5 - vPh) / 0.18);

  bool tdsDirty  = (tdsValue > TDS_THRESHOLD);
  bool turbDirty = (ntu > NTU_THRESHOLD);
  bool phDirty   = (phValue < PH_LOW_LIMIT || phValue > PH_HIGH_LIMIT);

  bool waterDirty = tdsDirty || turbDirty || phDirty;

  if (waterDirty) {
    runMotor(200);
  } else {
    runMotor(0);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("Water Quality Monitor");

  display.drawLine(0, 10, 128, 10, WHITE);

  display.setCursor(0, 15);
  display.print("TDS: ");
  display.print((int)tdsValue);

  display.setCursor(0, 25);
  display.print("NTU: ");
  display.print((int)ntu);

  display.setCursor(0, 35);
  display.print("pH: ");
  display.print(phValue, 2);

  display.setCursor(0, 50);
  display.print("STATUS: ");
  display.print(waterDirty ? "DIRTY" : "CLEAN");

  display.display();

  Serial.print("TDS: "); Serial.print(tdsValue);
  Serial.print(" | NTU: "); Serial.print(ntu);
  Serial.print(" | pH: "); Serial.println(phValue);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = String(server) + "?api_key=" + apiKey +
                 "&field1=" + String(tdsValue) +
                 "&field2=" + String(ntu) +
                 "&field3=" + String(phValue);

    http.begin(url);
    http.GET();
    http.end();
  }

  delay(15000);
}

void runMotor(int speed) {

  if (speed > 0) {
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
  } else {
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
  }

  ledcWrite(MOTOR_ENA, speed);
}