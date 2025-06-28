#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Seeed_FS.h>
#include <SD/Seeed_SD.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "config.h"
#include "speech_to_text.h"

class TextToSpeech {
public:
void convertTextToSpeech(String text)
{
    DynamicJsonDocument doc(1024);
doc["language"] = LANGUAGE;
doc["voice"] = _voice;
doc["text"] = text;

String body;
serializeJson(doc, body);
HTTPClient httpClient;
httpClient.begin(_client, TEXT_TO_SPEECH_FUNCTION_URL);

int httpResponseCode = httpClient.POST(body);
if (httpResponseCode == 200)
{            
    File wav_file = SD.open("SPEECH.WAV", FILE_WRITE);
    httpClient.writeToStream(&wav_file);
    wav_file.close();
}
else
{
    Serial.print("Failed to get speech - error ");
    Serial.println(httpResponseCode);
}
httpClient.end();
}
    void init() {
        // Prepare JSON body
        DynamicJsonDocument doc(1024);
        doc["language"] = LANGUAGE;
        String body;
        serializeJson(doc, body);

        // Make HTTP request to get voice list
        HTTPClient httpClient;
        httpClient.begin(_client, GET_VOICES_FUNCTION_URL);
        int httpResponseCode = httpClient.POST(body);

        if (httpResponseCode == 200) {
            String result = httpClient.getString();
            Serial.println(result);

            DynamicJsonDocument responseDoc(1024);
            deserializeJson(responseDoc, result.c_str());

            JsonArray voices = responseDoc.as<JsonArray>();
            _voice = voices[0].as<String>();

            Serial.print("Using voice: ");
            Serial.println(_voice);
        } else {
            Serial.print("Failed to get voices - error ");
            Serial.println(httpResponseCode);
        }

        httpClient.end();
    }

private:
    WiFiClient _client;
    String _voice;
};

// Global instance
TextToSpeech textToSpeech;
