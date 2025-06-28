#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <az_core.h>
#include <az_iot.h>

#include "config_private_gps.h"  // IOT_HUB_HOST, IOT_HUB_DEVICE_ID, IOT_HUB_SAS_TOKEN, SSID, PASSWORD
#include "ntp.h"

az_iot_hub_client az_client;
WiFiClientSecure wifi_client;
PubSubClient mqtt_client(wifi_client);

char mqtt_client_id[128];
char mqtt_username[128];

// Use UART2 (Serial2) on ESP32, mapped to GPIO16 (RX), GPIO17 (TX)
HardwareSerial GPS(2);  // UART2
TinyGPSPlus gps;        // TinyGPSPlus to process NMEA sentences
#define SLEEP_TIME 30   // Time to sleep in seconds (30 seconds)
#define GPS_TIMEOUT 10000   // Maximum time to wait for GPS data (in milliseconds)

void connectWiFi() {
  // First disconnect to reset everything
  
  WiFi.disconnect();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected!");
}

void setup_azure_client() {
  Serial.println("Setting up Azure client");

  az_iot_hub_client_options options = az_iot_hub_client_options_default();
  az_result rc = az_iot_hub_client_init(
    &az_client,
    az_span_create((uint8_t*)IOT_HUB_HOSTNAME, strlen(IOT_HUB_HOSTNAME)),
    az_span_create((uint8_t*)IOT_HUB_DEVICE_ID, strlen(IOT_HUB_DEVICE_ID)),
    &options
  );

  if (az_result_failed(rc)) {
    Serial.println("Azure client init failed!");
    return;
  }

  size_t username_length;
  rc = az_iot_hub_client_get_user_name(
    &az_client,
    mqtt_username,
    sizeof(mqtt_username),
    &username_length
  );
  if (az_result_failed(rc)) {
    Serial.println("Get username failed!");
    return;
  }
  
  mqtt_username[username_length] = '\0';  // Null-terminate (should already be fine, but safe to do)

  rc = az_iot_hub_client_get_client_id(
    &az_client,
    mqtt_client_id,
    sizeof(mqtt_client_id),
    NULL
  );

  if (az_result_failed(rc)) {
    Serial.println("Get client ID failed!");
    return;
  }
}

void handleMessage(char* topic, byte* payload, unsigned int length) {
  Serial.print("C2D Message on topic: ");
  Serial.println(topic);

  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("Payload: " + message);

  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.println("JSON parse error!");
    return;
  }
}

void connectMQTT() {
  mqtt_client.setServer(IOT_HUB_HOSTNAME, 8883);
  mqtt_client.setCallback(handleMessage);
  // These are important 
  mqtt_client.setSocketTimeout(60);  
  mqtt_client.setBufferSize(1024);   
  mqtt_client.setKeepAlive(60);      

  wifi_client.setInsecure();  // Required for Azure

  while (!mqtt_client.connected()) {
    Serial.println("Connecting to Azure MQTT...");
    if (mqtt_client.connect(mqtt_client_id, mqtt_username, IOT_HUB_SAS_TOKEN)) {
      Serial.println("MQTT connected!");
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.println(mqtt_client.state());
      delay(2000);
    }
  }

  // Subscribe to C2D topic   
  mqtt_client.subscribe("devices/" IOT_HUB_DEVICE_ID "/messages/devicebound/#");
}

bool readGPSData() {
  bool newData = false;
  
  // Read data from GPS module
  for (unsigned long start = millis(); millis() - start < 1000;) {
    while (GPS.available()) {
      char c = GPS.read();
      gps.encode(c);  // Feed data to TinyGPSPlus decoder
      newData = true;
    }
  }
  
  // If we processed new data and have a valid location, print it
  if (newData && gps.location.isValid()) {
    Serial.print("Location: ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
    Serial.print(" - from ");
    Serial.print(gps.satellites.value());
    Serial.println(" satellites");
    return true;
  }
  
  return false;
}

void sendTelemetry() {
  char topic[128];
  char payload[128];

  StaticJsonDocument<128> doc;
  doc["lat"] = gps.location.lat();
  doc["lng"] = gps.location.lng();
  serializeJson(doc, payload);

  // Get the telemetry topic
  az_result result = az_iot_hub_client_telemetry_get_publish_topic(
    &az_client, 
    NULL,  // No message properties
    topic, 
    sizeof(topic), 
    NULL   // No need to get topic length
  );

  if (az_result_failed(result)) {
    Serial.println("Failed to get MQTT topic for telemetry!");
    return;
  }

  if (mqtt_client.publish(topic, payload)) {
    Serial.println("Telemetry sent: " + String(payload));
  } else {
    Serial.println("Failed to publish telemetry!");
  }
}



unsigned long lastSend = 0;
void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for serial port to connect (needed for native USB)
    delay(10);
  }
  
  Serial.println("\nESP32 GPS Azure IoT Hub Tracker Starting...");

  // Initialize GPS
  GPS.begin(9600, SERIAL_8N1, 16, 17);  // GPS at 9600 baud
  Serial.println("GPS initialized");
  
  // Connect to WiFi and set up Azure
  connectWiFi();
  initTime();  // NTP time sync (assuming this is defined in ntp.h)
  setup_azure_client();
  connectMQTT();
  
  Serial.println("Waiting for GPS data...");
  
  // Try to get GPS data for up to GPS_TIMEOUT milliseconds
  bool gpsDataReceived = false;
  unsigned long startTime = millis();
  
  while ((millis() - startTime) < GPS_TIMEOUT) {
    if (readGPSData()) {
      gpsDataReceived = true;
      break;
    }
    
    // Keep the MQTT connection alive while waiting for GPS
    if (!mqtt_client.connected()) {
      connectMQTT();
    }
    mqtt_client.loop();
    
    delay(100);  // Small delay between reads
  }
  
  if (gpsDataReceived) {
    Serial.println("Valid GPS data received, sending to Azure IoT Hub");
    // Send the GPS data to Azure IoT Hub
    sendTelemetry();
  } else {
    Serial.println("No valid GPS data received within timeout period.");
  }
  
  // Process any incoming messages
  for (int i = 0; i < 10; i++) {  // Process messages for a short time
    mqtt_client.loop();
    delay(100);
  }
  
  // Disconnect MQTT cleanly before sleep
  mqtt_client.disconnect();
  
  // Print a message before going to sleep
  Serial.println("Going to sleep for " + String(SLEEP_TIME) + " seconds...");
  Serial.flush();  // Make sure all serial data is sent before sleeping
  delay(100);
  
  // Initialize deep sleep timer for the specified duration
  esp_sleep_enable_timer_wakeup(SLEEP_TIME * 1000000);  // SLEEP_TIME in microseconds
  esp_deep_sleep_start();
}

void loop() {
  // This won't be reached because we're using deep sleep
  // But it's good practice to have it for potential future changes
  delay(1000);
}
