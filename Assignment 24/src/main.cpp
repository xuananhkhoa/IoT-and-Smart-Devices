#include <sfud.h>
#include <SPI.h>
#include <rpcWiFi.h>
#include <arduino-timer.h>
#include "text_to_speech.h"
#include "config.h"
#include "mic.h"
#include "speech_to_text.h"
#include "language_understanding.h" 
#include "text_translator.h"
// Global instances
Mic mic;
SpeechToText speechToText;
LanguageUnderstanding languageUnderstanding;
auto timer = timer_create_default();

// Connect to WiFi
void connectWiFi() {
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connecting to WiFi..");
        WiFi.begin(SSID, PASSWORD);
        delay(500);
    }
    Serial.println("Connected!");
}

// Text-to-speech simulation
void say(const String& text) {
    text = textTranslator.translateText(text, LANGUAGE, SERVER_LANGUAGE);
    Serial.print("Saying: ");
    Serial.println(text);
    textToSpeech.convertTextToSpeech(text);
}

// Timer callback
bool timerExpired(void* announcement) {
    say((char*)announcement);
    return false;  // Do not repeat
}

// Process recorded audio and set timer
void processAudio() {
    Serial.println("Processing audio...");
    String text = speechToText.convertSpeechToText();
    Serial.println("Recognized Text: " + text);

    int total_seconds = languageUnderstanding.GetTimerDuration(text);
    if (total_seconds == 0) return;

    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;

    // Build begin message
    String begin_message;
    if (minutes > 0) begin_message += String(minutes) + " minute ";
    if (seconds > 0) begin_message += String(seconds) + " second ";
    begin_message += "timer started.";

    // Build end message
    String end_message = "Time's up on your ";
    if (minutes > 0) end_message += String(minutes) + " minute ";
    if (seconds > 0) end_message += String(seconds) + " second ";
    end_message += "timer.";

    say(begin_message);
    timer.in(total_seconds * 1000, timerExpired, (void*)end_message.c_str());
    String text = speechToText.convertSpeechToText();
    text = textTranslator.translateText(text, LANGUAGE, SERVER_LANGUAGE);
}

// Setup system
void setup() {
    Serial.begin(115200);
    while (!Serial);  // Wait for Serial

    connectWiFi();

    // Initialize flash
    while (sfud_init() != SFUD_SUCCESS);
    sfud_qspi_fast_read_enable(sfud_get_device(SFUD_W25Q32_DEVICE_INDEX), 2);

    pinMode(WIO_KEY_C, INPUT_PULLUP);

    mic.init();
    speechToText.init();
    textToSpeech.init();

    Serial.println("Ready.");
}

// Main loop
void loop() {
    if (digitalRead(WIO_KEY_C) == LOW && !mic.isRecording()) {
        Serial.println("Starting recording...");
        mic.startRecording();
    }

    if (!mic.isRecording() && mic.isRecordingReady()) {
        Serial.println("Finished recording");
        processAudio();
        mic.reset();
    }

    timer.tick();  // Handle timer events
}
