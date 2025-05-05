import json
import time

# Downgrade to less than version 2.0 to prevent errors
import paho.mqtt.client as mqtt

'''
app.py id must match with ID of config.h.
Ensures both the ESP32 and Python script communicate on the same topic.
'''
id = "99fbca6d-21c3-4669-a466-aa9a41f16236"

# MQTT topic where the ESP32 sends light level data.
client_telemetry_topic = id + '/telemetry'
# MQTT topic to send commands to
server_command_topic = id + '/commands'
# A unique name for this MQTT client.
client_name = id + 'nightlight_server'

# Creates an MQTT client and connects to a public MQTT broker
# CallbackAPIVersion should be VERSION1 to prevent errors
mqtt_client = mqtt.Client(client_name)
mqtt_client.connect('test.mosquitto.org')

# Starts a background loop to continuously check for MQTT messages.
mqtt_client.loop_start()

'''
Function runs when a message is received.
Decodes the received message from JSON format.
Prints the light level data received from ESP32.
'''
def handle_telemetry(client, userdata, message):
    payload = json.loads(message.payload.decode())
    print("Message received:", payload)

    # If light is less than 300, led_on is True to turn on light
    command = { 'led_on' : payload['light'] < 300 }
    print("Sending message:", command)
    client.publish(server_command_topic, json.dumps(command))

'''
Subscribes to the same telemetry topic .
Assigns the function handle_telemetry to run when a message is received.
'''
mqtt_client.subscribe(client_telemetry_topic)
mqtt_client.on_message = handle_telemetry

'''
Keeps the program running forever.
The MQTT client listens in the background, so this just prevents the script from stopping.
'''
while True:
    time.sleep(2)