/*
  ESP32 Mini Weather Station & Early Flood Detection (v3 - With Alerts)
  ----------------------------------------------------------------------
  Features:
    - A dynamic notification panel at the top for critical alerts.
    - Status for Rain, Wind, and River level (e.g., Low, High, Flood Alert).
    - Wind Speed monitoring (analog value).
    - Rain water level measurement in millimeters (mm).
    - Organized ESPDash dashboard with distinct sections.
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
#define DHTPIN 14
#define DHTTYPE DHT22
#define RAIN_SENSOR_PIN 34
#define WIND_SPEED_PIN 35
#define RAIN_PUMP_RELAY_PIN 12
#define RIVER_PUMP_RELAY_PIN 13
#define WATER_TRIG_PIN 26
#define WATER_ECHO_PIN 27
#define RAIN_LEVEL_TRIG_PIN 32
#define RAIN_LEVEL_ECHO_PIN 33

// --- Global Objects ---
DHT dht(DHTPIN, DHTTYPE);
AsyncWebServer server(80);
ESPDash dashboard(server);
unsigned long lastUpdate = 0;

// --- Dashboard Cards ---

// NEW: Section for System Alerts at the top
dash::SeparatorCard alertSeparator(dashboard, "System Alerts");
dash::FeedbackCard<String> rainAlertCard(dashboard, "Rain Status");
dash::FeedbackCard<String> windAlertCard(dashboard, "Wind Condition");
dash::FeedbackCard<String> riverAlertCard(dashboard, "River Condition");

// Section: Wind & Atmosphere
dash::SeparatorCard envSeparator(dashboard, "Wind & Atmosphere");
dash::GenericCard<int> windSpeedCard(dashboard, "Wind Speed (km/h)");
dash::TemperatureCard temperatureCard(dashboard, "Temperature");
dash::HumidityCard humidityCard(dashboard, "Humidity");

// Section: Rainfall Monitoring
dash::SeparatorCard rainSeparator(dashboard, "Rainfall Monitoring");
dash::GenericCard<String> rainSensorCard(dashboard, "Rain Detection");
dash::FeedbackCard<String> rainLevelCard(dashboard, "Rain Water Level (mm)");
dash::ToggleButtonCard rainPumpSwitch(dashboard, "Rain Pump");

// Section: River Monitoring
dash::SeparatorCard riverSeparator(dashboard, "River Monitoring");
dash::FeedbackCard<String> riverLevelCard(dashboard, "River Water Level (mm)");
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

  long duration = pulseIn(echoPin, HIGH, 30000);
  float distance = duration * 0.0343 / 2;
  return distance;
}


void setup() {
  Serial.begin(115200);

  pinMode(RAIN_PUMP_RELAY_PIN, OUTPUT);
  pinMode(RIVER_PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(RAIN_PUMP_RELAY_PIN, LOW);
  digitalWrite(RIVER_PUMP_RELAY_PIN, LOW);

  dht.begin();

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

  rainPumpSwitch.onChange([](bool state) {
    digitalWrite(RAIN_PUMP_RELAY_PIN, state);
    dashboard.sendUpdates();
  });

  riverPumpSwitch.onChange([](bool state) {
    digitalWrite(RIVER_PUMP_RELAY_PIN, state);
    dashboard.sendUpdates();
  });

  server.begin();

  // Set initial feedback for alert cards
  rainAlertCard.setFeedback("Normal", dash::Status::SUCCESS);
  windAlertCard.setFeedback("Calm", dash::Status::SUCCESS);
  riverAlertCard.setFeedback("Normal", dash::Status::SUCCESS);
}

void loop() {
  if (millis() - lastUpdate >= 2000) {

    // --- Read All Sensor Data ---
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    int windValue = analogRead(WIND_SPEED_PIN);
    int rainValue = analogRead(RAIN_SENSOR_PIN);
    
    // Calculate water levels in mm
    // Assuming total height of rain container is 100mm
    int rainLevelMM = 100 - (int)(getDistanceCM(RAIN_LEVEL_TRIG_PIN, RAIN_LEVEL_ECHO_PIN) * 10.0);
    // Assuming total height of river container is 150mm
    int riverLevelMM = 140 - (int)(getDistanceCM(WATER_TRIG_PIN, WATER_ECHO_PIN) * 10.0);


    // --- Update Alert Panel based on sensor values ---

    // 1. Rain Alert Logic (Lower analog value means more rain)
    if (rainValue < 1500) {
      rainAlertCard.setFeedback("High", dash::Status::DANGER);
    } else if (rainValue < 3000) {
      rainAlertCard.setFeedback("Medium", dash::Status::WARNING);
    } else if (rainValue < 3800) {
      rainAlertCard.setFeedback("Low", dash::Status::INFO);
    } else {
      rainAlertCard.setFeedback("No Rain", dash::Status::SUCCESS);
    }

    // 2. Wind Speed Alert Logic (Higher analog value means more speed)
    if (windValue > 150) {
      windAlertCard.setFeedback("High", dash::Status::DANGER);
    } else if (windValue > 100) {
      windAlertCard.setFeedback("Medium", dash::Status::WARNING);
    } else {
      windAlertCard.setFeedback("Low", dash::Status::SUCCESS);
    }

    // 3. River Level & Flood Alert Logic
    if (riverLevelMM > 100) { // Example: Flood risk if level is over 120mm
      riverAlertCard.setFeedback("Flood Alert!", dash::Status::DANGER);
    } else if (riverLevelMM > 50) { // Example: High level warning
      riverAlertCard.setFeedback("High Level", dash::Status::WARNING);
    } else {
      riverAlertCard.setFeedback("Normal", dash::Status::SUCCESS);
    }


    // --- Update Regular Dashboard Cards ---

    // Wind & Atmosphere Section
    windSpeedCard.setValue(windValue);
    if (!isnan(h) && !isnan(t)) {
      temperatureCard.setValue(t);
      humidityCard.setValue(h);
    }

    // Rainfall Monitoring Section
    rainSensorCard.setValue(rainValue < 3800 ? "Raining" : "Not Raining");
    rainLevelCard.setFeedback(String(rainLevelMM), dash::Status::INFO);

    // River Monitoring Section
    riverLevelCard.setFeedback(String(riverLevelMM), dash::Status::INFO);


    // Send all updates to the dashboard
    dashboard.sendUpdates();
    lastUpdate = millis();
  }
}
