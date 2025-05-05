#include <Arduino.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJSON.h>
#include <math.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <SPI.h>

#include "config.h"

// Define DHT sensor type and pin
#define DHTPIN 4     // GPIO4 - change this to your connected pin
#define DHTTYPE DHT11 // DHT 11

void connectWiFi()  // Function to connect to WiFi
{
    Serial.println("Connecting to WiFi...");

    WiFi.disconnect(true);  // Ensure previous connections are cleared
    delay(1000);

    WiFi.begin(SSID, PASSWORD);  // Start WiFi connection

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // Retry for 20 seconds
        Serial.print(".");
        delay(1000);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi!");
        // If you want to check your IP address, uncomment this code
        /*
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        */        
    } else {
        Serial.println("\nFailed to connect. Check WiFi settings.");
    }
}

WiFiClient espClient;         // Create a WiFi client for ESP32
PubSubClient client(espClient); // Create an MQTT client using WiFi


unsigned long lastReconnectAttempt = 0;  // Track last reconnect attempt
const long reconnectInterval = 5000;     // Retry every 5 seconds
void reconnectMQTTClient()
{
  if (client.connected()) return;  // Exit if already connected, only attempt to connect when neccessary
  
  if (millis() - lastReconnectAttempt >= reconnectInterval) {
    lastReconnectAttempt = millis();  // Update last attempt time

    Serial.print("Attempting MQTT connection...");
    if (client.connect(CLIENT_NAME.c_str())) {
        Serial.println("connected");
        
        // ✅ Resubscribe after reconnecting
        client.subscribe(SERVER_COMMAND_TOPIC.c_str());
        Serial.println("Subscribed to command topic.");
    } else {
        Serial.print("Failed, rc=");
        Serial.println(client.state());
    }
  }
}

void createMQTTClient()
{
    /*BROKER.c_str() → Converts BROKER (a std::string) to const char*, which setServer() requires.
    1883 → The default port for MQTT communication.*/
    client.setServer(BROKER.c_str(), 1883);
    
    /*Calls reconnectMQTTClient(), which tries to establish a connection to the MQTT broker.*/
    reconnectMQTTClient();
}

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Use millis to better handle latency
unsigned long lastTelemetryTime = 0;  // Tracks last send time
const long telemetryInterval = 10000;  // Send telemetry every 10 minutes

void setup() {
    Serial.begin(9600);
    while (!Serial) {
      ; // Wait for serial port to connect
    }

    // Start dht
    dht.begin();
    
    // Attempt to connect to WiFi
    connectWiFi();

    // Attempt to create a MQTT client and connectto MQTT
    createMQTTClient();
}

void loop() {
    // 1. Prioritize MQTT connection
    if (!client.connected()) {
      reconnectMQTTClient();
    }
    client.loop();

    // 2. Skip if WiFi is down
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected. Skipping telemetry.");
      delay(1000);
      return;
    }

    // 3. Send telemetry at intervals
    if (millis() - lastTelemetryTime >= telemetryInterval) {
      lastTelemetryTime = millis();

      float tempC = dht.readTemperature();
      float tempF = dht.readTemperature(true);

      if (isnan(tempC) || isnan(tempF)) {
        Serial.println("⚠️ DHT11 read failed!");
        return;
      }

      DynamicJsonDocument doc(1024);
      doc["tempC"] = tempC;
      doc["tempF"] = tempF;  // Optional: Send both C and F

      String telemetry;
      serializeJson(doc, telemetry);

      Serial.print("Sending: ");
      Serial.println(telemetry);

      bool published = client.publish(CLIENT_TELEMETRY_TOPIC.c_str(), telemetry.c_str(), 1);
      if (!published) {
        Serial.println("⚠️ MQTT publish failed!");
      }
    }
}