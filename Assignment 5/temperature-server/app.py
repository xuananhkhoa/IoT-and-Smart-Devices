import json
import time
import paho.mqtt.client as mqtt
from os import path
import csv
from datetime import datetime

id = "2d85d648-2b06-40f5-a556-9e12aca7329f"
client_telemetry_topic = id + '/telemetry'
server_command_topic = id + '/commands'
client_name = id + 'temperature_sensor_server'

# Update fieldnames to match what you're writing
temperature_file_name = 'temperature.csv'
fieldnames = ['date', 'tempC', 'tempF']  # Fixed field names

# Create CSV file if it doesn't exist
if not path.exists(temperature_file_name):
    with open(temperature_file_name, mode='w') as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
        writer.writeheader()

def handle_telemetry(client, userdata, message):
    try:
        payload = json.loads(message.payload.decode())
        print(f"📩 Message received: {payload}")
        
        # Safely extract temperatures with defaults
        tempC = payload.get('tempC', 999)
        tempF = payload.get('tempF', 999)
        
        # Use logical `and` instead of bitwise `&`
        if tempC != 999 and tempF != 999:  # ✅ Fixed condition
            with open(temperature_file_name, mode='a') as temperature_file:        
                temperature_writer = csv.DictWriter(temperature_file, fieldnames=fieldnames)
                temperature_writer.writerow({
                    'date': datetime.now().astimezone().replace(microsecond=0).isoformat(),
                    'tempC': tempC,
                    'tempF': tempF
                })
            print("✅ Data written to CSV.")
        else:
            print("⚠️ Warning: Temperature data missing in payload.")
    except json.JSONDecodeError:
        print("❌ Error: Invalid JSON payload.")

# MQTT Setup
mqtt_client = mqtt.Client(client_name)
mqtt_client.on_message = handle_telemetry

print("🔗 Connecting to MQTT broker...")
mqtt_client.connect('test.mosquitto.org')
mqtt_client.subscribe(client_telemetry_topic, qos=1)

print(f"📡 Listening for messages on topic: {client_telemetry_topic}")
mqtt_client.loop_forever()