#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include "SD/Seeed_SD.h"
#include <Seeed_FS.h>
#include "camera.h"
#include "config.h"

#include <ArduinoJSON.h>

Camera camera = Camera(JPEG, OV2640_640x480);
int fileNum = 1;

void setupSDCard()
{
    while (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI))
    {
        Serial.println("SD Card Error");
    }
}

void saveToSDCard(byte *buffer, uint32_t length)
{
    char buff[16];
    sprintf(buff, "%d.jpg", fileNum);
    fileNum++;

    File outFile = SD.open(buff, FILE_WRITE );
    outFile.write(buffer, length);
    outFile.close();

    Serial.print("Image written to file ");
    Serial.println(buff);
}

void setupCamera()
{
    pinMode(PIN_SPI_SS, OUTPUT);
    digitalWrite(PIN_SPI_SS, HIGH);

    Wire.begin();
    SPI.begin();

    if (!camera.init())
    {
        Serial.println("Error setting up the camera!");
    }
}

void classifyImage(byte *buffer, uint32_t length)
{
    HTTPClient httpClient;
    httpClient.begin(client, PREDICTION_URL);
    httpClient.addHeader("Content-Type", "application/octet-stream");
    httpClient.addHeader("Prediction-Key", PREDICTION_KEY);

    int httpResponseCode = httpClient.POST(buffer, length);

    if (httpResponseCode == 200)
    {
        String result = httpClient.getString();

        DynamicJsonDocument doc(1024);
        deserializeJson(doc, result.c_str());

        JsonObject obj = doc.as<JsonObject>();
        JsonArray predictions = obj["predictions"].as<JsonArray>();

        for(JsonVariant prediction : predictions) 
        {
            String tag = prediction["tagName"].as<String>();
            float probability = prediction["probability"].as<float>();

            char buff[32];
            sprintf(buff, "%s:\t%.2f%%", tag.c_str(), probability * 100.0);
            Serial.println(buff);
        }
    }

    httpClient.end();
}

void buttonPressed()
{
    camera.startCapture();

    while (!camera.captureReady())
        delay(100);

    Serial.println("Image captured");

    byte *buffer;
    uint32_t length;

    if (camera.readImageToBuffer(&buffer, length))
    {
        Serial.print("Image read to buffer with length ");
        Serial.println(length);

        saveToSDCard(buffer, length);
        
        classifyImage(buffer, length);

        delete(buffer);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected!");

    setupSDCard();
    setupCamera();

    pinMode(WIO_KEY_C, INPUT_PULLUP);
}

void loop()
{
    if (digitalRead(WIO_KEY_C) == LOW)
    {
        buttonPressed();
        delay(2000);
    }

    delay(200);
}
