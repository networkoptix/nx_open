## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import asyncio

from util.base_host_connector import BaseHostConnector, nx_integration_interface
from util.integration_settings import IntegrationSettings
import integration.detail.mqtt_client as mqtt_client

ACTION_ID = "nx.mqtt.proxy.publishAction"


# Extend the class with the methods that can be called from the c++ code.
class HostConnector(BaseHostConnector):
    def __init__(self, settings_model: IntegrationSettings):
        super().__init__(settings_model)
        self._mqtt_client = None

    def set_mqtt_client(self, client: mqtt_client.paho.Client):
        self._mqtt_client = client

    def object_actions_model(self, _) -> str:
        return f"""
[
    {{
        "id": "{ACTION_ID}",
        "name": "MQTT Proxy: Publish Message",
        "supportedObjectTypeIds": [],
        "parametersModel":
        {{
            "type": "Settings",
            "items":
            [
                {{
                    "type": "TextField",
                    "name": "topic",
                    "caption": "MQTT Topic",
                    "description": "MQTT topic to publish the message to"
                }},
                {{
                    "type": "TextArea",
                    "name": "message",
                    "caption": "Message to publish",
                    "description": "MQTT message body to publish"
                }},
                {{
                    "type": "SpinBox",
                    "name": "qos",
                    "caption": "QoS Level",
                    "defaultValue": 0,
                    "minValue": 0,
                    "maxValue": 2
                }},
                {{
                    "type": "CheckBox",
                    "name": "retain",
                    "caption": "Retain",
                    "description": "Turn on message retention on the broker",
                    "defaultValue": false
                }},
                {{
                    "type": "TextField",
                    "name": "correlation_data",
                    "caption": "Correlation data",
                    "description": "Correlation data to be sent with the message"
                }}
            ]
        }}
    }}
]
"""

    def execute_object_action(self, params: dict[str, str]) -> str:
        if params.get('_action_id') != ACTION_ID:
            return super().execute_object_action(params)

        if not self._mqtt_client:
            return "Error: MQTT client is not bound"

        result, status = asyncio.run(mqtt_client.publish_message(self._mqtt_client, params))
        return "Message published" if status else f"Error: {result}"
