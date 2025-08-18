/*
  ESPDASH Lite - Dual Ultrasonic & DHT Sensor Example
  -------------------------------------------------------
  Features:
    - Organized layout with Separator Cards
    - Measures River Water Level with HC-SR04
    - Measures Rain Level with a second HC-SR04
    - Real-time Temperature & Humidity from DHT sensor
    - LED control via Toggle Button
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include <DHT.h>

/* WiFi Credentials */
const char* ssid = "Mehedi";
const char* password = "44332211";

/* DHT Sensor Config */
#define DHTPIN 14      // GPIO pin for DHT data
#define DHTTYPE DHT11  // or DHT22
DHT dht(DHTPIN, DHTTYPE);

/* Ultrasonic Sensor 1 (River Water Level) Config */
#define WATER_TRIG_PIN 26    // GPIO for Trigger
#define WATER_ECHO_PIN 27    // GPIO for Echo

/* Ultrasonic Sensor 2 (Rain Level) Config */
#define RAIN_TRIG_PIN 32     // GPIO for Trigger
#define RAIN_ECHO_PIN 33     // GPIO for Echo

/* Webserver and Dashboard */
AsyncWebServer server(80);
ESPDash dashboard(server);

/* Dashboard Cards 
  The order here defines the order on the dashboard.
*/
dash::ToggleButtonCard button(dashboard, "LED");

// *** NEW: Separator for environmental sensors ***
dash::SeparatorCard envSeparator(dashboard, "Atmosphere Sensors"); 

dash::TemperatureCard temperature(dashboard, "Temperature");
dash::HumidityCard humidity(dashboard, "Humidity");

// *** NEW: Separator for water level sensors ***
dash::SeparatorCard waterSeparator(dashboard, "Water Level Sensors");

dash::FeedbackCard<String> waterLevelCard(dashboard, "River Water Level (cm)");
dash::FeedbackCard<String> rainLevelCard(dashboard, "Rain Level (cm)");


/* Timer for updates */
unsigned long lastUpdate = 0;

/* Reusable function to read distance from any HC-SR04 sensor */
float getDistanceCM(int trigPin, int echoPin) {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); // timeout 30ms
  float distance = duration * 0.0343 / 2; // cm
  return distance;
}

void setup() {
  Serial.begin(115200);
  pinMode(12, OUTPUT);       // LED

  /* Start DHT sensor */
  dht.begin();

  /* Connect to WiFi */
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

  /* Button Callback */
  button.onChange([](bool state) {
    Serial.println(String("Button Triggered: ") + (state ? "ON" : "OFF"));
    button.setValue(state);
    digitalWrite(12, state);
    dashboard.sendUpdates();
  });

  /* Start Server */
  server.begin();

  /* Set initial feedback for both cards */
  waterLevelCard.setFeedback("0", dash::Status::INFO);
  rainLevelCard.setFeedback("0", dash::Status::INFO);
}

void loop() {
  unsigned long now = millis();
  if (now - lastUpdate >= 1000) { // Every 1 second

    /* Read DHT sensor */
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      temperature.setValue(t);
      humidity.setValue(h);
      Serial.printf("Temp: %.2f °C | Hum: %.2f %%\n", t, h);
    } else {
      Serial.println("Failed to read from DHT sensor!");
    }

    /* Read River Water Level Sensor */
    float waterDistance = getDistanceCM(WATER_TRIG_PIN, WATER_ECHO_PIN);
    int waterDistanceInt = (int)waterDistance;
    String waterLevelStr = String(waterDistanceInt);
    Serial.printf("River Water Level: %d cm\n", waterDistanceInt);
    
    if (waterDistanceInt < 10) {
      waterLevelCard.setFeedback(waterLevelStr, dash::Status::DANGER);
    } else if (waterDistanceInt < 30) {
      waterLevelCard.setFeedback(waterLevelStr, dash::Status::WARNING);
    } else {
      waterLevelCard.setFeedback(waterLevelStr, dash::Status::SUCCESS);
    }

    /* Read Rain Level Sensor */
    float rainDistance = getDistanceCM(RAIN_TRIG_PIN, RAIN_ECHO_PIN);
    int rainDistanceInt = (int)rainDistance;
    String rainLevelStr = String(rainDistanceInt);
    Serial.printf("Rain Level: %d cm\n", rainDistanceInt);
    
    if (rainDistanceInt > 50) { 
      rainLevelCard.setFeedback(rainLevelStr, dash::Status::DANGER);
    } else if (rainDistanceInt > 25) {
      rainLevelCard.setFeedback(rainLevelStr, dash::Status::WARNING);
    } else {
      rainLevelCard.setFeedback(rainLevelStr, dash::Status::SUCCESS);
    }

    /* Send all updates to dashboard */
    dashboard.sendUpdates();
    lastUpdate = now;
  }
}
