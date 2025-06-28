#include <time.h>
#include <WiFi.h>

// NTP Server settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 6 * 3600;  // GMT offset (in seconds) for GMT+7
const int daylightOffset_sec = 3600;  // Daylight savings offset (in seconds)

static void initTime(){
    // Initialize and sync time with NTP server
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Wait for time to be synchronized
    Serial.println("Waiting for NTP time sync...");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2){
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }

    struct tm timeinfo;
    getLocalTime(&timeinfo);
    Serial.println(&timeinfo, "Time synchronized: %A, %B %d %Y %H:%M:%S");
}