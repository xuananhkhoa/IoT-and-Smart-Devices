#pragma once

#include <string>

using namespace std;

// WiFi credentials
const char* SSID = "Khoa";
const char *PASSWORD = "Khoa22122005";

// MQTT settings
const string ID = "2d85d648-2b06-40f5-a556-9e12aca7329f";
const string BROKER = "test.mosquitto.org";
const string CLIENT_NAME = ID + "temperature_sensor_client";

// Telemetry topic (send readings)
const string CLIENT_TELEMETRY_TOPIC = ID + "/telemetry";

// Command topic (receive commands)
const string SERVER_COMMAND_TOPIC = ID + "/commands";
