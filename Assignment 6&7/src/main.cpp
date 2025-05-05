#include <Arduino.h>
#include <ArduinoJSON.h>
#include <math.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <SPI.h>

#include "config.h"


#define SOIL_MOISTURE_PIN 34  // Use GPIO34 for analog input of soil moisture sensor
#define PIN_WIRE_SCL 26 // Use GPIO26 for digital output  of relay
#define ABSOLUTE_DRYNESS 4095

int calculate_soil_moisture_percentage(int soil_moisture)
{
  // Beware of integer division
    // int percentage {100 - round((soil_moisture / float(ABSOLUTE_DRYNESS) ) * 100)};

    /*Or use the map function
    - Map absolute drought to 0 (2 <--> 4)
    - Map absolute flood to 100 (3 <--> 5)
    */
  return map(soil_moisture, ABSOLUTE_DRYNESS, 0, 0, 100);
}

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

void clientCallback(char *topic, uint8_t *payload, unsigned int length)
{
    /*payload is a raw byte array, since MQTT protocol transmits messages as binary data
    This is copied into buff, a null-terminated C string, making it easier to process.
    The length is provided by clientCallback itself, so we can just use it*/
    char buff[length + 1];
    for (int i = 0; i < length; i++)
    {
        buff[i] = (char)payload[i];
    }
    buff[length] = '\0';

    Serial.print("Message received:");
    Serial.println(buff);

    /*Convert buff into JSON format
    If not, buff could look like: '{\"relay_on\": true}', which is still a string*/
    DynamicJsonDocument doc(1024);
    /*Here we are attempting to parse buff (a JSON string) into doc (a DynamicJsonDocument object)
    - If successful, doc now contains the parsed JSON data
    - If failed, deserializeJson() returns an error object, which is caught by error.*/
    DeserializationError error = deserializeJson(doc, buff);
    if (error) {
      Serial.println("⚠️ JSON parsing failed!");
      return;
    }
    
    /*doc is still but a dynamic JSON document. Need to objectify it further*/
    JsonObject obj = doc.as<JsonObject>();

    // Extract the true/false value for relay, then toggle it on or off accordingly
    bool relay_on = obj["relay_on"];

    if (relay_on)
        digitalWrite(PIN_WIRE_SCL, HIGH);
    else
        digitalWrite(PIN_WIRE_SCL, LOW);
}

void createMQTTClient()
{
    /*BROKER.c_str() → Converts BROKER (a std::string) to const char*, which setServer() requires.
    1883 → The default port for MQTT communication.*/
    client.setServer(BROKER.c_str(), 1883);
    
    /*Calls reconnectMQTTClient(), which tries to establish a connection to the MQTT broker.*/
    reconnectMQTTClient();

    /*Calls clientCallback when a message is received
    No need to manually pass arguments for clientCallback, setCallback from PubSubClient handles that for you*/
    client.setCallback(clientCallback);
}

// Use millis to better handle latency
unsigned long lastTelemetryTime = 0;  // Tracks last send time
const long telemetryInterval = 10000;  // Send telemetry every 2 seconds

void setup() {
    Serial.begin(9600); // Higher baud rate for ESP32
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(PIN_WIRE_SCL, OUTPUT);

    // Attempt to connect to WiFi
    connectWiFi();

    // Attempt to create a MQTT client and connectto MQTT
    createMQTTClient();
}

void loop() {
    // Only attempt to reconnect if disconnected
    reconnectMQTTClient();

    client.loop();

    // millis() returns the number of miliseconds passed since the program started running
    if (millis() - lastTelemetryTime >= telemetryInterval)
    {
      lastTelemetryTime = millis();  // Update last checkpoint to now 

      // Analog output
      int soil_moisture = calculate_soil_moisture_percentage(analogRead(SOIL_MOISTURE_PIN));

      /*Creates a JSON document with a maximum size of 1024 bytes
      Stores the light sensor value in the JSON document under the "light" key*/
      DynamicJsonDocument doc(1024);
      doc["soil_moisture"] = soil_moisture;
      
      
      /*serializeJson() converts the JSON object into a String (Arduino supported string format)
      Fills the empty telemetry string with JSON data by serializing the doc object.*/ 
      String telemetry;
      serializeJson(doc, telemetry);


      Serial.print("Sending telementry: ");
      Serial.println(telemetry.c_str());

      // Send sensor readings to telemetry topic
      client.publish(CLIENT_TELEMETRY_TOPIC.c_str(), telemetry.c_str(), 1);
    }
}

