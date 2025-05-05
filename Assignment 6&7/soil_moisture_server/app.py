import json
import time

import paho.mqtt.client as mqtt
import threading

# Make threshold configurable
DRYNESS_THRESHOLD = 15  # Percent

id = "192a986b-602f-4092-be87-89f3bc80e9b0"

# Same telemetry topic, command topic and client name
client_telemetry_topic = id + '/telemetry'
server_command_topic = id + '/commands'
client_name = id + 'soil_moisture_server'

water_time = 5 # Open the relay for 5 seconds
wait_time = 20 # Wait 20 seconds for water to soak before try again

def send_relay_command(client, state):
    command = {'relay_on' : state}
    print(f"📡 Sending command: {command}")
    client.publish(server_command_topic, json.dumps(command))

def control_relay(client):
    # Temporarily unsubscribe from telementry
    print("Unsubscribing from telemetry")
    mqtt_client.unsubscribe(client_telemetry_topic)

    # Send command to turn on relay, wait 5 seconds, then turn in off
    send_relay_command(client, True)
    time.sleep(water_time)
    send_relay_command(client, False)

    # Wait 20 seconds after turning off relay to check for moisture again
    time.sleep(wait_time)

    # Resubscribe to telemetry to get moisture readings
    print("Subscribing to telemetry")
    mqtt_client.subscribe(client_telemetry_topic)


# Callback function when a message is received
'''
What the 3 params of handle_telemetry represent:
- client :  the instance of the mqtt.Client() object that received the message.
- userdata : user-defined object that Allows you to pass custom data, default is None.
- message : An object of type MQTTMessage containing
    * message.topic (str) – The topic the message was received on.
    * message.payload (bytes) – The actual message data (needs .decode() for text).
    * message.qos (int) – Quality of Service level (0, 1, or 2).
    * message.retain (bool) – Whether this is a retained message.
'''
def handle_telemetry(client, userdata, message):
    try:
        payload = json.loads(message.payload.decode())
        print(f"📩 Message received: {payload}")  # Use f-string for better readability

        # Extract soil moisture safely, defaulting to 999 (impossible value)
        # Use .get (Python directionary) to prevent crashes
        soil_moisture = payload.get('soil_moisture', 999)
        
        # Only send command if soil_moisture is valid
        if soil_moisture != 999:
            if soil_moisture < DRYNESS_THRESHOLD:
                # Threading starts a new thread to handle time-consuming tasks (watering and waiting for 25sec)
                '''
                target : the function that the new thread should call
                args : arguments for the target function
                The (client, ) syntax is used to create a 1-element tuple in Python''' 
                threading.Thread(target=control_relay, args=(client,)).start()
        else:
            print("⚠️ Warning: 'soil_moisture' missing in payload.")

    except json.JSONDecodeError:
        print("❌ Error: Received message is not valid JSON.")

'''
Create MQTT client
CallbackAPIVersion should be VERSION1 to prevent errors 
'''
mqtt_client = mqtt.Client(client_name)

# Set callback function before connecting. Callback tells server what to do upon getting a message
# Callback first → Before receiving messages, firstdefine what to do when a message arrives.
# No need to pass arguments manually, library does it for you
mqtt_client.on_message = handle_telemetry

# Connect to MQTT broker
# Connect second → Connection must first be establised before subscribing.
print("🔗 Connecting to MQTT broker...")
mqtt_client.connect('test.mosquitto.org')


# Subscribe to the topic with QoS level 1 (At Least Once)
# Subscribe third → Once connected, server is ready to receive messages on this topic"
mqtt_client.subscribe(client_telemetry_topic, qos=1)

# Start MQTT loop (blocking, better than while True)
# Loop last → loop_forever() keeps listening for incoming messages.
print(f"📡 Listening for messages on topic: {client_telemetry_topic}")
mqtt_client.loop_forever()


