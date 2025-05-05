#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <az_core.h>
#include <az_iot.h>

#include "config_private.h"  // IOT_HUB_HOST, IOT_HUB_DEVICE_ID, IOT_HUB_SAS_TOKEN, SSID, PASSWORD
#include "ntp.h"

#define SOIL_MOISTURE_PIN 34
#define RELAY_PIN 26
#define ABSOLUTE_DRYNESS 4095

az_iot_hub_client az_client;
WiFiClientSecure wifi_client;
PubSubClient mqtt_client(wifi_client);

char mqtt_client_id[128];
char mqtt_username[128];

bool relayActive = false;

void connectWiFi() {
  WiFi.begin(SSID, PASSWORD);
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
    az_span_create((uint8_t*)IOT_HUB_HOST, strlen(IOT_HUB_HOST)),
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

  if (doc.containsKey("relay")) {
    bool state = doc["relay"];
    digitalWrite(RELAY_PIN, state ? HIGH : LOW);
    relayActive = state;
    Serial.println(String("Relay state set to ") + (state ? "ON" : "OFF"));
  }
}


void connectMQTT() {
  mqtt_client.setServer(IOT_HUB_HOST, 8883);
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

int calculate_soil_moisture_percentage(int reading) {
  return map(reading, ABSOLUTE_DRYNESS, 0, 0, 100);
}

void sendTelemetry() {
  char topic[128];
  char payload[128];

  int moisture = analogRead(SOIL_MOISTURE_PIN);
  int percentage = calculate_soil_moisture_percentage(moisture);

  StaticJsonDocument<128> doc;
  doc["soil-moisture"] = percentage;
  serializeJson(doc, payload);

  az_span telemetry_topic;
  az_result result = az_iot_hub_client_telemetry_get_publish_topic(&az_client, NULL, topic, sizeof(topic), NULL);

  if (az_result_failed(result)) {
    Serial.println("Failed to get MQTT topic for telemetry!");
    return;
  }

  mqtt_client.publish(topic, payload);
  Serial.println("Telemetry sent: " + String(payload));
}


unsigned long lastSend = 0;

void setup() {
  Serial.begin(9600);
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  connectWiFi();
  initTime();
  setup_azure_client();
  connectMQTT();
}

void loop() {
  if (!mqtt_client.connected()) {
    connectMQTT();
  }
  mqtt_client.loop();

  if (millis() - lastSend > 10000) {
    sendTelemetry();
    lastSend = millis();
  }

  delay(10);
}
