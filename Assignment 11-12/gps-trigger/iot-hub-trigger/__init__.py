import logging
from azure.functions import EventHubEvent

import json
import os
import uuid
from azure.storage.blob import BlobServiceClient, PublicAccess

def main(event: EventHubEvent):  # No "s" – it's just one event
    logging.info('Python EventHub trigger processed an event: %s',
                 event.get_body().decode('utf-8'))
    
    device_id = event.iothub_metadata['connection-device-id']
    blob_name = f'{device_id}/{str(uuid.uuid1())}.json'

    container_client = get_or_create_container('gps-data')
    blob = container_client.get_blob_client(blob_name)

    event_body = json.loads(event.get_body().decode('utf-8'))

     # Option 1: If you want to keep the same structure in your code
    gps = {
        'gps': {
            'lat': event_body['lat'],
            'lng': event_body['lng']
        }
    }

    blob_body = {
        'device_id' : device_id,
        'timestamp' : event.iothub_metadata['enqueuedtime'],
        'gps': gps
    }

    logging.info(f'Writing blob to {blob_name} - {blob_body}')
    blob.upload_blob(json.dumps(blob_body).encode('utf-8'))


def get_or_create_container(name):
    connection_str = os.environ['STORAGE_CONNECTION_STRING']
    blob_service_client = BlobServiceClient.from_connection_string(connection_str)

    for container in blob_service_client.list_containers():
        if container.name == name:
            return blob_service_client.get_container_client(container.name)
    
    return blob_service_client.create_container(name)