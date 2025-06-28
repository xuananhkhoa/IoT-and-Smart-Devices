#include <Arduino.h>
#include <rpcWiFi.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include "SD/Seeed_SD.h"
#include <Seeed_FS.h>
#include "Seeed_vl53l0x.h"
#include "config.h"
#include "camera.h"

WiFiClientSecure client;
Camera camera = Camera(JPEG, OV2640_640x480);
Seeed_vl53l0x VL53L0X;
int fileNum = 1;

// ========================== Sensor Setup ==========================
void setupSensor() {
    VL53L0X.VL53L0X_common_init();
    VL53L0X.VL53L0X_high_accuracy_ranging_init();

    VL53L0X_RangingMeasurementData_t RangingMeasurementData;
    memset(&RangingMeasurementData, 0, sizeof(RangingMeasurementData));

    VL53L0X.PerformSingleRangingMeasurement(&RangingMeasurementData);

    Serial.print("Distance = ");
    Serial.print(RangingMeasurementData.RangeMilliMeter);
    Serial.println(" mm");

    delay(1000);
}

// ========================== Camera Setup ==========================
void setupCamera() {
    pinMode(PIN_SPI_SS, OUTPUT);
    digitalWrite(PIN_SPI_SS, HIGH);

    Wire.begin();
    SPI.begin();

    if (!camera.init()) {
        Serial.println("Error setting up the camera!");
    }
}

// ========================== WiFi Setup ==========================
void connectWiFi() {
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connecting to WiFi..");
        WiFi.begin(SSID, PASSWORD);
        delay(500);
    }
    Serial.println("Connected!");
}

// ========================== SD Card Setup ==========================
void setupSDCard() {
    while (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
        Serial.println("SD Card Error");
    }
}

// ========================== Save Image to SD ==========================
void saveToSDCard(byte *buffer, uint32_t length) {
    char filename[16];
    sprintf(filename, "%d.jpg", fileNum++);
    File outFile = SD.open(filename, FILE_WRITE);
    outFile.write(buffer, length);
    outFile.close();

    Serial.print("Image written to file ");
    Serial.println(filename);
}

// ========================== Image Classification ==========================
void classifyImage(byte *buffer, uint32_t length) {
    HTTPClient httpClient;
    httpClient.begin(client, PREDICTION_URL);
    httpClient.addHeader("Content-Type", "application/octet-stream");
    httpClient.addHeader("Prediction-Key", PREDICTION_KEY);

    int httpResponseCode = httpClient.POST(buffer, length);

    if (httpResponseCode == 200) {
        String result = httpClient.getString();

        DynamicJsonDocument doc(1024);
        deserializeJson(doc, result.c_str());

        JsonArray predictions = doc["predictions"].as<JsonArray>();
        for (JsonVariant prediction : predictions) {
            String tag = prediction["tagName"].as<String>();
            float probability = prediction["probability"].as<float>();

            char output[32];
            sprintf(output, "%s:\t%.2f%%", tag.c_str(), probability * 100.0);
            Serial.println(output);
        }
    }

    httpClient.end();
}

// ========================== Capture on Button Press ==========================
void buttonPressed() {
    camera.startCapture();

    while (!camera.captureReady())
        delay(100);

    Serial.println("Image captured");

    byte *buffer;
    uint32_t length;

    if (camera.readImageToBuffer(&buffer, length)) {
        Serial.print("Image read to buffer with length ");
        Serial.println(length);

        saveToSDCard(buffer, length);
        // classifyImage(buffer, length); // Optional: uncomment to classify

        delete(buffer);
    }
}

// ========================== Arduino Setup ==========================
void setup() {
    Serial.begin(9600);
    while (!Serial); // Wait for Serial to be ready

    delay(1000);

    connectWiFi();
    setupCamera();
    pinMode(WIO_KEY_C, INPUT_PULLUP);
    setupSDCard();
    setupSensor();
}

// ========================== Arduino Loop ==========================
void loop() {
    if (digitalRead(WIO_KEY_C) == LOW) {
        buttonPressed();
        delay(2000); // debounce + delay
    }
    delay(200);
}
