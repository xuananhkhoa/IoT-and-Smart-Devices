#include <Arduino.h>
#include <ArduinoJSON.h>
#include <math.h>
#include <AdafruitIO_WiFi.h>
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
   
  soil_moisture = constrain(soil_moisture, 0, ABSOLUTE_DRYNESS);  // Force soil_moisture to be within 0 and 4095
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
    } else {
        Serial.println("\nFailed to connect. Check WiFi settings.");
    }
}

WiFiClient espClient;         // Create a WiFi client for ESP32
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, SSID, PASSWORD);  // Create an instance of the AdafruitIO_WiFi object
AdafruitIO_Feed *soil_moisture_feed = io.feed(SOIL_FEED_NAME); // soil_moisture_feed
AdafruitIO_Feed *relay_command_feed = io.feed(RELAY_FEED_NAME); // relay_command_feed


// Use millis to better handle latency
unsigned long last_connecting_time = 0;  // Tracks last send time
const long connecting_interval = 1000;  // Send telemetry every 2 seconds

// Function to connect to Adafruit IO
void connect_to_Adafruit() {
  Serial.println("Connecting to Adafruit IO...");
  
  // Initialize the Adafruit IO client
  io.connect();

  // Try to reconnect once every 5 seconds
  unsigned long startAttemptTime = millis();

  // io.status() returns a value that's less than AIO_CONNECTED if connection failed
  while (io.status() < AIO_CONNECTED && millis() - startAttemptTime < 5000) {
    if (millis() - last_connecting_time >= connecting_interval) {
        last_connecting_time = millis();
        Serial.print(".");
    }
  }
  Serial.println("Connected to Adafruit IO");
}

void send_telemetry(AdafruitIO_Feed* feed, int soil_moisture){
  Serial.print("Sending message: ");
  Serial.println(soil_moisture);

  // Publish to feed
  feed->save(soil_moisture);
}

// Callback function to handle commands
void handleCommand(AdafruitIO_Data *data) {
  // Get data value
  String command = data->value();
  command.trim();
  command.toLowerCase();  // Case-insensitive handling

  Serial.print("Received command: ");
  Serial.println(command);
  

  if (command == "relay_on") {
    digitalWrite(PIN_WIRE_SCL, HIGH);
  } 
  else if (command == "relay_off") {
    digitalWrite(PIN_WIRE_SCL, LOW);
  }
}

void setup() {
    Serial.begin(9600); // Baud rate for ESP32

    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(PIN_WIRE_SCL, OUTPUT);

    // Attempt to connect to WiFi
    connectWiFi();

    // Attempt to connect to AdafruitIO
    connect_to_Adafruit();

    // Set up command feedback
    relay_command_feed->onMessage(handleCommand);
    
}

// Use millis to better handle latency
unsigned long last_sensor_read = 0;  // Tracks last sensor read
const long telemetry_interval = 10000;  // Send telemetry every 10 seconds

// This is for when WiFi drops
unsigned long last_reconnect_attempt = 0;
const long reconnect_interval = 5000; // 5 seconds

void loop() {
    // Reconnect logic if connection drops, attempt to reconnect once every 5 seconds
    if ((WiFi.status() != WL_CONNECTED || io.status() < AIO_CONNECTED) &&
    millis() - last_reconnect_attempt > reconnect_interval) {
      last_reconnect_attempt = millis();
      connectWiFi();
      connect_to_Adafruit();
    }
    
    // Call io.run() to keep Adafruit IO running and processing messages
    io.run();

    // millis() returns the number of miliseconds passed since the program started running
    if (millis() - last_sensor_read >= telemetry_interval)
    {
      last_sensor_read = millis();  // Update last checkpoint to now 

      // Analog output
      int soil_moisture = calculate_soil_moisture_percentage(analogRead(SOIL_MOISTURE_PIN));

      // Sending telemetry
      send_telemetry(soil_moisture_feed, soil_moisture);  
    }
}

