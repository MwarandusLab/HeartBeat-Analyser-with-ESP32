#include <Wire.h>
#include <WiFi.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <HTTPClient.h>
#include <LiquidCrystal.h>

MAX30105 particleSensor;

const byte RATE_SIZE = 4;  //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE];     //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0;  //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;
long irValue;
int DataisReady = 0;
float temperature;
float temperatureF;

enum State {
  IDLE,
  MEASURE_TEMPERATURE,
  MEASURE_BP
};

State currentState = IDLE;
unsigned long measurementStartTime;
unsigned long DataSendStartTime;

//const int rs = 32, en = 33, d4 = 25, d5 = 26, d6 = 27, d7 = 14;
const int rs = 14, en = 27, d4 = 26, d5 = 25, d6 = 33, d7 = 32;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

WiFiClientSecure client;

const char *ssid = "Wi-Fi SSID";
const char *password = "password";

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  WiFi.begin(ssid, password);

  lcd.begin(16, 2);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi: Connecting");
    lcd.setCursor(0, 1);
    lcd.print("IP: Loading...");
    delay(3000);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi: Connected");
  lcd.setCursor(0, 1);
  lcd.print("IP: " + WiFi.localIP().toString());
  delay(3000);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 was not found. Please check wiring/power. ");
    while (1)
      ;
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");
  particleSensor.setup();                     // Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A);  // Turn Red LED to low to indicate the sensor is running
  particleSensor.setPulseAmplitudeGreen(0);   // Turn off Green LED

  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("SYSTEM");
  lcd.setCursor(1, 1);
  lcd.print("INITIALIZATION");
  delay(2000);
}

void loop() {
  switch (currentState) {
    case IDLE:
      checkFingerDetection();
      break;

    case MEASURE_TEMPERATURE:
      measureTemperature();
      break;

    case MEASURE_BP:
      measureBP();
      break;
  }
}
void checkFingerDetection() {
  irValue = particleSensor.getIR();

  if (irValue > 50000) {
    currentState = MEASURE_TEMPERATURE;
    DataisReady = 0;
    measurementStartTime = millis();
  } else {
    lcd.clear();
    lcd.setCursor(5, 0);
    lcd.print("PLACE");
    lcd.setCursor(2, 1);
    lcd.print("YOUR FINGER");
    delay(1000);
  }
}
void measureTemperature() {
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("MEASURING");
  lcd.setCursor(1, 1);
  lcd.print("TEMPERATURE...");
  delay(1000);
  particleSensor.setup(0);  // Configure sensor. Turn off LEDs
  particleSensor.enableDIETEMPRDY();
  temperature = particleSensor.readTemperature();
  Serial.print("temperatureC = ");
  Serial.print(temperature, 2);

  temperatureF = particleSensor.readTemperatureF();
  Serial.print("   temperatureF = ");
  Serial.print(temperatureF, 2);

  Serial.println();

  // Check if 10 seconds have passed
  if (millis() - measurementStartTime >= 10000) {
    // Before transitioning to MEASURE_BP, re-initialize the sensor settings
    //sendTagToServer(temperature, beatAvg);
    particleSensor.setup();                     // Configure sensor with default settings
    particleSensor.setPulseAmplitudeRed(0x0A);  // Turn Red LED to low to indicate the sensor is running
    particleSensor.setPulseAmplitudeGreen(0);   // Turn off Green LED
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TEMP: ");
    lcd.print(temperature, 2);
    lcd.print((char)223);  // print ° character
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("MEASURING BP...");
    delay(1000);
    currentState = MEASURE_BP;
  }
}
void measureBP() {
  irValue = particleSensor.getIR();
  //Serial.println(irValue);
  if (irValue > 50000) {
    if (checkForBeat(irValue) == true) {
      // We sensed a beat!
      long delta = millis() - lastBeat;
      lastBeat = millis();

      beatsPerMinute = 60 / (delta / 1000.0);

      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        rates[rateSpot++] = (byte)beatsPerMinute;  // Store this reading in the array
        rateSpot %= RATE_SIZE;                     // Wrap variable

        // Take the average of readings
        beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;

        if (irValue < 50000)
          Serial.print(" No finger?");

        Serial.println();
        if (beatAvg > 65 && beatAvg < 80 && beatsPerMinute > 65) {
          // Send the BPM data via GSM
          sendTagToServer(temperature, beatAvg);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("TEMP: ");
          lcd.print(temperature, 2);
          lcd.print((char)223);  // print ° character
          lcd.print("C");
          lcd.setCursor(0, 1);
          lcd.print("Avg BP: ");
          lcd.print(beatAvg);

          Serial.print("IR=");
          Serial.print(irValue);
          Serial.print(", BPM=");
          Serial.print(beatsPerMinute);
          Serial.print(", Avg BPM=");
          Serial.print(beatAvg);
          delay(1000);

          DataisReady = 1;
          DataSendStartTime = millis();
        }
      }
    }
  }
  if (DataisReady == 1) {
    unsigned long TimeRemaining = millis() - DataSendStartTime;
    Serial.print("TimeRemaining: ");
    Serial.println(TimeRemaining / 1000);
    delay(2000);
    if (TimeRemaining >= 10000) {
      currentState = IDLE;
      Serial.println("CurrentState Changed to IDLE");
    }
  }
  if (irValue < 6000) {
    Serial.println("No Finger Detected");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TEMP: ");
    lcd.print(temperature, 2);
    lcd.print((char)223);  // print ° character
    lcd.print("C");
    lcd.setCursor(0, 1);
    lcd.print("FINGER REMOVED");
  }
}

void sendTagToServer(float temperature, int beatAvg) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String serverURL = "https://example.com/index.php";
  http.begin(client, serverURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // Prepare data to send to the server
  String postData = "temperature=" + String(temperature, 1) + "&beatAvg=" + String(beatAvg);

  int httpResponseCode = http.POST(postData);
  if (httpResponseCode > 0) {
    Serial.printf("Data sent to server. HTTP Response code: %d\n", httpResponseCode);
    Serial.println("Data Send Successfully");
    while (!client.available()) {
      delay(10);
    }

    http.end();
  }
}
