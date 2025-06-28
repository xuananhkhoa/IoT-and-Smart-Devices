#include <vector>
#include <ArduinoJSON.h>
#include "SD/Seeed_SD.h"
#include <Seeed_FS.h>
#include <Arduino.h>
#include "camera.h"
#include <WiFiClientSecure.h>
WiFiClientSecure client;
Camera camera = Camera(JPEG, OV2640_640x480);

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

void setupSDCard()
{
    while (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI))
    {
        Serial.println("SD Card Error");
    }
}

void setup() {

  setupCamera();
  pinMode(WIO_KEY_C, INPUT_PULLUP);
  setupSDCard();
}

int fileNum = 1;

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

const float overlap_threshold = 0.20f;
struct Point {
    float x, y;
};

struct Rect {
    Point topLeft, bottomRight;
};

float area(Rect rect)
{
    return abs(rect.bottomRight.x - rect.topLeft.x) * abs(rect.bottomRight.y - rect.topLeft.y);
}
 
float overlappingArea(Rect rect1, Rect rect2)
{
    float left = max(rect1.topLeft.x, rect2.topLeft.x);
    float right = min(rect1.bottomRight.x, rect2.bottomRight.x);
    float top = max(rect1.topLeft.y, rect2.topLeft.y);
    float bottom = min(rect1.bottomRight.y, rect2.bottomRight.y);


    if ( right > left && bottom > top )
    {
        return (right-left)*(bottom-top);
    }
    
    return 0.0f;
}

Rect rectFromBoundingBox(JsonVariant prediction)
{
    JsonObject bounding_box = prediction["boundingBox"].as<JsonObject>();

    float left = bounding_box["left"].as<float>();
    float top = bounding_box["top"].as<float>();
    float width = bounding_box["width"].as<float>();
    float height = bounding_box["height"].as<float>();

    Point topLeft = {left, top};
    Point bottomRight = {left + width, top + height};

    return {topLeft, bottomRight};
}

void processPredictions(std::vector<JsonVariant> &predictions)
{
    std::vector<JsonVariant> passed_predictions;

    for (int i = 0; i < predictions.size(); ++i)
    {
        Rect prediction_1_rect = rectFromBoundingBox(predictions[i]);
        float prediction_1_area = area(prediction_1_rect);
        bool passed = true;

        for (int j = i + 1; j < predictions.size(); ++j)
        {
            Rect prediction_2_rect = rectFromBoundingBox(predictions[j]);
            float prediction_2_area = area(prediction_2_rect);

            float overlap = overlappingArea(prediction_1_rect, prediction_2_rect);
            float smallest_area = min(prediction_1_area, prediction_2_area);

            if (overlap > (overlap_threshold * smallest_area))
            {
                passed = false;
                break;
            }
        }

        if (passed)
        {
            passed_predictions.push_back(predictions[i]);
        }
    }
    for(JsonVariant prediction : passed_predictions)
    {
        String boundingBox = prediction["boundingBox"].as<String>();
        String tag = prediction["tagName"].as<String>();
        float probability = prediction["probability"].as<float>();

        char buff[32];
        sprintf(buff, "%s:\t%.2f%%\t%s", tag.c_str(), probability * 100.0, boundingBox.c_str());
        Serial.println(buff);
    }
    Serial.print("Counted ");
    Serial.print(passed_predictions.size());
    Serial.println(" stock items.");
}

void detectStock (byte *buffer, uint32_t length)
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

       std::vector<JsonVariant> passed_predictions;

    for(JsonVariant prediction : predictions) 
    {
        float probability = prediction["probability"].as<float>();
        if (probability > threshold)
        {
            passed_predictions.push_back(prediction);
        }
    }

processPredictions(passed_predictions);
    }

    httpClient.end();
}

void buttonPressed()
{
  Serial.print("Image read to buffer with length ");
  Serial.println(length);

  saveToSDCard(buffer, length);

  delete(buffer);

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

      delete(buffer);
  }
  detectStock(buffer, length);
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


