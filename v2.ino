/*
  ESP32 Mini Weather Station & Early Flood Detection (v2)
  ---------------------------------------------------------
  Features:
    - Wind Speed monitoring (analog value).
    - Rain water level measurement in millimeters (mm).
    - Organized ESPDash dashboard with distinct sections.
    - Rain detection and river water level measurement.
    - DHT22 for temperature and humidity readings.
    - Independent toggle switches to control pumps.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include <DHT.h>

// --- WiFi Credentials ---
const char* ssid = "Mehedi";
const char* password = "44332211";

// --- Pin Definitions ---
// DHT Sensor
#define DHTPIN 14
#define DHTTYPE DHT22

// Analog Input Sensors
#define RAIN_SENSOR_PIN 34
#define WIND_SPEED_PIN 35  // New: Pin for Wind Speed Sensor

// Relay Pins for Pumps
#define RAIN_PUMP_RELAY_PIN 12
#define RIVER_PUMP_RELAY_PIN 13

// Ultrasonic Sensor 1 (River Water Level)
#define WATER_TRIG_PIN 26
#define WATER_ECHO_PIN 27

// Ultrasonic Sensor 2 (Rain Level)
#define RAIN_LEVEL_TRIG_PIN 32
#define RAIN_LEVEL_ECHO_PIN 33

// --- Global Objects ---
DHT dht(DHTPIN, DHTTYPE);
AsyncWebServer server(80);
ESPDash dashboard(server);
unsigned long lastUpdate = 0;

// --- Dashboard Cards ---

// Section: Wind & Atmosphere
dash::SeparatorCard envSeparator(dashboard, "Wind & Atmosphere");
dash::GenericCard<int> windSpeedCard(dashboard, "Wind Speed", "Km/H"); // New: Card for Wind Speed
dash::TemperatureCard temperatureCard(dashboard, "Temperature");
dash::HumidityCard humidityCard(dashboard, "Humidity");

// Section: Rainfall Monitoring
dash::SeparatorCard rainSeparator(dashboard, "Rainfall Monitoring");
dash::GenericCard<int> rainSensorCard(dashboard, "Rain Intensity", "Value");
dash::FeedbackCard<String> rainLevelCard(dashboard, "Rain Water Level (mm)"); // Updated: Unit changed to mm
dash::ToggleButtonCard rainPumpSwitch(dashboard, "Rain Pump");

// Section: River Monitoring
dash::SeparatorCard riverSeparator(dashboard, "River Monitoring");
dash::FeedbackCard<String> riverLevelCard(dashboard, "River Water Level (cm)");
dash::ToggleButtonCard riverPumpSwitch(dashboard, "River Pump");


// --- Reusable function to read distance from any HC-SR04 sensor ---
float getDistanceCM(int trigPin, int echoPin) {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
  float distance = duration * 0.0343 / 2;
  return distance;
}


void setup() {
  Serial.begin(115200);

  // Initialize Relay pins
  pinMode(RAIN_PUMP_RELAY_PIN, OUTPUT);
  pinMode(RIVER_PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(RAIN_PUMP_RELAY_PIN, LOW);
  digitalWrite(RIVER_PUMP_RELAY_PIN, LOW);

  dht.begin();

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Failed! Restarting...");
    delay(2000);
    ESP.restart();
  }
  Serial.print("\nConnected! IP Address: ");
  Serial.println(WiFi.localIP());

  // --- Card Callbacks ---
  rainPumpSwitch.onChange([](bool state) {
    Serial.println(String("Rain Pump Toggled: ") + (state ? "ON" : "OFF"));
    rainPumpSwitch.setValue(state);
    digitalWrite(RAIN_PUMP_RELAY_PIN, state);
    dashboard.sendUpdates();
  });

  riverPumpSwitch.onChange([](bool state) {
    Serial.println(String("River Pump Toggled: ") + (state ? "ON" : "OFF"));
    riverPumpSwitch.setValue(state);
    digitalWrite(RIVER_PUMP_RELAY_PIN, state);
    dashboard.sendUpdates();
  });

  server.begin();

  // Set initial feedback for cards
  riverLevelCard.setFeedback("0", dash::Status::INFO);
  rainLevelCard.setFeedback("0", dash::Status::INFO);
}

void loop() {
  // Update sensor data every 2 seconds
  if (millis() - lastUpdate >= 2000) {

    // --- Read Wind & Atmosphere Sensors ---
    int windValue = analogRead(WIND_SPEED_PIN); // New: Read wind sensor
    windSpeedCard.setValue(windValue);
    Serial.printf("Wind Speed (Analog): %d\n", windValue);

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      temperatureCard.setValue(t);
      humidityCard.setValue(h);
      Serial.printf("Temp: %.2f C | Hum: %.2f %%\n", t, h);
    } else {
      Serial.println("Failed to read from DHT sensor!");
    }

    // --- Read Rainfall Monitoring Sensors ---
    int rainValue = analogRead(RAIN_SENSOR_PIN);
    rainSensorCard.setValue(rainValue);
    Serial.printf("Rain Sensor Value: %d\n", rainValue);

    // Updated: Calculate distance in mm (cm * 10)
    float rainDistanceMM = getDistanceCM(RAIN_LEVEL_TRIG_PIN, RAIN_LEVEL_ECHO_PIN) * 10.0;
    int rainDistanceInt = 99 - (int)rainDistanceMM;
    rainLevelCard.setFeedback(String(rainDistanceInt), dash::Status::INFO);
    Serial.printf("Rain Water Level: %d mm\n", rainDistanceInt);
    
    // --- Read River Monitoring Sensor ---
    float riverDistance = getDistanceCM(WATER_TRIG_PIN, WATER_ECHO_PIN) * 10;
    int riverDistanceInt = 150 - (int)riverDistance;
    Serial.printf("River Water Level: %d mm\n", riverDistanceInt);
    
    if (riverDistanceInt < 10) {
      riverLevelCard.setFeedback(String(riverDistanceInt), dash::Status::DANGER);
    } else if (riverDistanceInt < 30) {
      riverLevelCard.setFeedback(String(riverDistanceInt), dash::Status::WARNING);
    } else {
      riverLevelCard.setFeedback(String(riverDistanceInt), dash::Status::SUCCESS);
    }

    dashboard.sendUpdates();
    lastUpdate = millis();
  }
}
