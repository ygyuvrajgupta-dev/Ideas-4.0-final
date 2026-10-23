/*
  SAATHI Smart Agriculture System
  Main ESP32 Dev Module

  DHT22 DATA -> GPIO 4
  Soil sensor AO -> GPIO 34
  Relay IN -> GPIO 26

  ThingSpeak:
  Field 1 = Temperature
  Field 2 = Humidity
  Field 3 = Soil raw value
  Field 4 = Pump status (1 ON, 0 OFF)
*/

#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <ThingSpeak.h>
#include "secrets.h.example"

#define DHT_PIN 4
#define DHT_TYPE DHT22
#define SOIL_PIN 34
#define RELAY_PIN 26

// Verify relay polarity with pump disconnected.
// true = active-LOW, false = active-HIGH.
const bool RELAY_ACTIVE_LOW = true;

// Example thresholds ONLY. Calibrate with your sensor.
int SOIL_DRY_THRESHOLD = 2800;
int SOIL_WET_THRESHOLD = 2300;

const unsigned long SENSOR_INTERVAL_MS = 2000;
const unsigned long THINGSPEAK_INTERVAL_MS = 20000;
const unsigned long MAX_PUMP_RUN_MS = 30000;

DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);
WiFiClient thingSpeakClient;

bool pumpIsOn = false;
bool automaticMode = false;
bool dhtReadingValid = false;

float temperatureC = NAN;
float humidityPercent = NAN;
int soilRaw = 0;

unsigned long lastSensorRead = 0;
unsigned long lastThingSpeakWrite = 0;
unsigned long pumpStartedAt = 0;
unsigned long lastReconnectAttempt = 0;

// ---------- CORS ----------
void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Cache-Control", "no-store");
}

void sendText(int code, const String &message) {
  addCorsHeaders();
  server.send(code, "text/plain; charset=utf-8", message);
}

void handleOptions() {
  addCorsHeaders();
  server.send(204, "text/plain", "");
}

// ---------- Relay control ----------
void writeRelay(bool turnOn) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_PIN, turnOn ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN, turnOn ? HIGH : LOW);
  }
}

void setPump(bool turnOn, const char *reason) {
  if (pumpIsOn == turnOn) {
    return;
  }

  pumpIsOn = turnOn;
  writeRelay(pumpIsOn);

  if (pumpIsOn) {
    pumpStartedAt = millis();
  }

  Serial.printf(
    "Pump %s (%s)\n",
    pumpIsOn ? "ON" : "OFF",
    reason
  );
}

// ---------- Web endpoints ----------
void handlePumpOn() {
  automaticMode = false;
  setPump(true, "manual web command");
  sendText(200, "ON");
}

void handlePumpOff() {
  automaticMode = false;
  setPump(false, "manual web command");
  sendText(200, "OFF");
}

void handleAuto() {
  automaticMode = true;
  sendText(200, "AUTO");
}

void handleManual() {
  automaticMode = false;
  sendText(200, "MANUAL");
}

void handleStatus() {
  String json = "{";

  json += "\"pump\":\"";
  json += (pumpIsOn ? "ON" : "OFF");
  json += "\",";

  json += "\"pumpStatus\":";
  json += (pumpIsOn ? "1" : "0");
  json += ",";

  json += "\"autoMode\":";
  json += (automaticMode ? "true" : "false");
  json += ",";

  json += "\"temperature\":";
  if (dhtReadingValid) {
    json += String(temperatureC, 1);
  } else {
    json += "null";
  }
  json += ",";

  json += "\"humidity\":";
  if (dhtReadingValid) {
    json += String(humidityPercent, 1);
  } else {
    json += "null";
  }
  json += ",";

  json += "\"soilRaw\":";
  json += String(soilRaw);
  json += ",";

  json += "\"dhtValid\":";
  json += (dhtReadingValid ? "true" : "false");
  json += ",";

  json += "\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\"}";

  addCorsHeaders();
  server.send(
    200,
    "application/json; charset=utf-8",
    json
  );
}

void handleNotFound() {
  if (server.method() == HTTP_OPTIONS) {
    handleOptions();
  } else {
    sendText(
      404,
      "Use /ON, /OFF, /STATUS, /AUTO or /MANUAL"
    );
  }
}

// ---------- Sensors and irrigation ----------
void readSensors() {
  soilRaw = analogRead(SOIL_PIN);

  float newTemperature = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  if (isnan(newTemperature) || isnan(newHumidity)) {
    dhtReadingValid = false;
    Serial.println(
      "DHT22 ERROR: check sensor type, wiring, power and GPIO4."
    );
  } else {
    temperatureC = newTemperature;
    humidityPercent = newHumidity;
    dhtReadingValid = true;
  }

  // Assumes a higher raw reading means drier soil.
  // Calibrate these thresholds before enabling automatic irrigation.
  if (automaticMode) {
    if (!pumpIsOn && soilRaw >= SOIL_DRY_THRESHOLD) {
      setPump(true, "AUTO: dry threshold");
    } else if (pumpIsOn && soilRaw <= SOIL_WET_THRESHOLD) {
      setPump(false, "AUTO: wet threshold");
    }
  }

  // Maximum continuous pump runtime safety cutoff.
  if (pumpIsOn &&
      millis() - pumpStartedAt >= MAX_PUMP_RUN_MS) {
    setPump(false, "safety timeout");
    Serial.println("SAFETY: pump stopped after 30 seconds.");
  }

  Serial.print("Temperature: ");
  if (dhtReadingValid) {
    Serial.print(temperatureC, 1);
  } else {
    Serial.print("INVALID");
  }

  Serial.print(" C | Humidity: ");
  if (dhtReadingValid) {
    Serial.print(humidityPercent, 1);
  } else {
    Serial.print("INVALID");
  }

  Serial.print(" % | Soil raw: ");
  Serial.print(soilRaw);

  Serial.print(" | Pump: ");
  Serial.print(pumpIsOn ? "ON" : "OFF");

  Serial.print(" | Mode: ");
  Serial.println(automaticMode ? "AUTO" : "MANUAL");
}

// ---------- ThingSpeak ----------
void writeThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ThingSpeak skipped: Wi-Fi disconnected.");
    return;
  }

  if (THINGSPEAK_CHANNEL_ID == 0 ||
      String(THINGSPEAK_WRITE_KEY).length() == 0 ||
      String(THINGSPEAK_WRITE_KEY) ==
        "REPLACE_WITH_YOUR_THINGSPEAK_WRITE_KEY") {
    Serial.println(
      "ThingSpeak skipped: configure secrets.h."
    );
    return;
  }

  if (!dhtReadingValid) {
    Serial.println(
      "ThingSpeak skipped: DHT22 reading is invalid."
    );
    return;
  }

  ThingSpeak.setField(1, temperatureC);
  ThingSpeak.setField(2, humidityPercent);
  ThingSpeak.setField(3, soilRaw);
  ThingSpeak.setField(4, pumpIsOn ? 1 : 0);

  int responseCode = ThingSpeak.writeFields(
    THINGSPEAK_CHANNEL_ID,
    THINGSPEAK_WRITE_KEY
  );

  Serial.printf(
    "ThingSpeak response code: %d\n",
    responseCode
  );

  if (responseCode != 200) {
    Serial.println(
      "ThingSpeak failed: check channel, write key, Wi-Fi and update limits."
    );
  }
}

// ---------- Wi-Fi ----------
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 30000UL) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected");

    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());

    ThingSpeak.begin(thingSpeakClient);
  } else {
    Serial.println(
      "Wi-Fi timeout. Check SSID/password and restart."
    );
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(300);

  // Request pump OFF at startup.
  // Confirm relay polarity before connecting the pump.
  pinMode(RELAY_PIN, OUTPUT);
  writeRelay(false);
  pumpIsOn = false;

  analogReadResolution(12);
  dht.begin();

  connectWiFi();

  server.on("/ON", HTTP_GET, handlePumpOn);
  server.on("/OFF", HTTP_GET, handlePumpOff);
  server.on("/STATUS", HTTP_GET, handleStatus);
  server.on("/AUTO", HTTP_GET, handleAuto);
  server.on("/MANUAL", HTTP_GET, handleManual);

  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("SAATHI web server started.");
  Serial.println(
    "Endpoints: /ON /OFF /STATUS /AUTO /MANUAL"
  );
  Serial.println(
    "Calibrate soil thresholds before using AUTO."
  );
}

// ---------- Main loop ----------
void loop() {
  server.handleClient();

  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    readSensors();
  }

  if (now - lastThingSpeakWrite >=
      THINGSPEAK_INTERVAL_MS) {
    lastThingSpeakWrite = now;
    writeThingSpeak();
  }

  if (WiFi.status() != WL_CONNECTED &&
      now - lastReconnectAttempt >= 10000UL) {
    lastReconnectAttempt = now;

    Serial.println(
      "Wi-Fi disconnected; trying to reconnect..."
    );

    WiFi.reconnect();
  }
}
