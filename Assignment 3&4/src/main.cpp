#include <Arduino.h>
#include <ArduinoJSON.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <SPI.h>
#include "config.h"

#define LED_PIN 5  // GPIO5 (D5) controls the Grove LED (digital) (output)
#define SENSOR_PIN 34 // GPIO34 (D34) is the light sensor (analog) (input)

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
void reconnectMQTTClient()
{
    /* Checks if the MQTT client is connected*/
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");

        /*Attempts to connect to the MQTT broker.
        Uses client.connect() to connect using CLIENT_NAME.
        CLIENT_NAME.c_str() converts CLIENT_NAME (a string) into a const char*, which connect() expects.*/
        if (client.connect(CLIENT_NAME.c_str()))
        {
            Serial.println("connected");
        }
        else
        {
            Serial.print("Retying in 5 seconds - failed, rc=");
            Serial.println(client.state());
            
            delay(5000);
        }
    }

    // Subscribe to the command topic when the MQTT client is reconnected
    client.subscribe(SERVER_COMMAND_TOPIC.c_str());
}

void createMQTTClient()
{
    /*BROKER.c_str() → Converts BROKER (a std::string) to const char*, which setServer() requires.
    1883 → The default port for MQTT communication.*/
    client.setServer(BROKER.c_str(), 1883);
    
    /*Calls reconnectMQTTClient(), which tries to establish a connection to the MQTT broker.*/
    reconnectMQTTClient();

    /*Calls clientCallback when a message is received*/
    client.setCallback(clientCallback);
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
    If not, buff could look like: '{\"led_on\": true}', which is still a string*/
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, buff);
    
    /*doc is still but a dynamic JSON document. Need to objectify it further*/
    JsonObject obj = doc.as<JsonObject>();

    bool led_on = obj["led_on"];

    if (led_on)
        digitalWrite(LED_PIN, HIGH);
    else
        digitalWrite(LED_PIN, LOW);
}

unsigned long lastTelemetryTime = 0;  // Tracks last send time
const long telemetryInterval = 2000;  // Send telemetry every 2 seconds

void setup() {
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(9600); // Start Serial Monitor
    connectWiFi();  // Call function to connect to WiFi
    createMQTTClient();
}

void loop()
{
   
    // Reconnect to the MQTT Broker if Disconnected
    reconnectMQTTClient();

    // Process MQTT Messages
    client.loop();
    
    // Send telemetry every 2 seconds (non-blocking)
    if (millis() - lastTelemetryTime >= telemetryInterval) {
        lastTelemetryTime = millis();  // Update time

        /*
        Read GPIO34 for analog output
        int light = analogRead(SENSOR_PIN);
        */
        
        // Simulate light level since I don't have a light sensor
        int light = esp_random() % 1024;

        /*Creates a JSON document with a maximum size of 1024 bytes
        Stores the light sensor value in the JSON document under the "light" key*/
        DynamicJsonDocument doc(1024);
        doc["light"] = light;

        /*serializeJson() converts the JSON object into a String (Arduino supported string format)
        Fills the empty telemetry string with JSON data by serializing the doc object.*/ 
        String telemetry;
        serializeJson(doc, telemetry);

        Serial.print("Sending telemetry ");
        Serial.println(telemetry.c_str());

        client.publish(CLIENT_TELEMETRY_TOPIC.c_str(), telemetry.c_str());
    }
}