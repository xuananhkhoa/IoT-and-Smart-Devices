#pragma once

#include <string>

using namespace std;

// WiFi credentials
const char* SSID = "ChoiSongJun";
const char *PASSWORD = "qvuw9726";

// MQTT settings
const string ID = "192a986b-602f-4092-be87-89f3bc80e9b0";
const string BROKER = "test.mosquitto.org";
const string CLIENT_NAME = ID + "soil_moisture_client";

// Telemetry topic (send readings)
const string CLIENT_TELEMETRY_TOPIC = ID + "/telemetry";

// Command topic (receive commands)
const string SERVER_COMMAND_TOPIC = ID + "/commands";