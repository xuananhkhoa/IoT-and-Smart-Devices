#pragma once

#include <string>
using namespace std;


const char* SSID = "Khoa";
const char* PASSWORD = "Khoa22122005";

const string ID = "99fbca6d-21c3-4669-a466-aa9a41f16236";
const string BROKER = "test.mosquitto.org";
const string CLIENT_NAME = ID + "nightlight_client";


// Define the telemetry topic name for the MQTT broker:
const string CLIENT_TELEMETRY_TOPIC = ID + "/telemetry";

// Define the command topic, which the device will subscribe to to receive LED commands.
const string SERVER_COMMAND_TOPIC = ID + "/commands";
