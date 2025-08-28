## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from typing import Optional
import enum
import json
import logging

from aiohttp import web
import paho.mqtt.client as paho

from util.integration_settings import IntegrationSettingsSection
from util.vms_http_client import VmsHttpClient

logger = logging.getLogger(f"mqtt_plugin.{__name__}")


class MqttPublishResult(enum.Enum):
    SUCCESS = enum.auto()
    BAD_PARAMETERS = enum.auto()
    PUBLISHING_ERROR = enum.auto()

    def __bool__(self):
        return self == MqttPublishResult.SUCCESS


def _connect_to_hivemq(settings: IntegrationSettingsSection) -> paho.Client:
    client = paho.Client(
        paho.CallbackAPIVersion.VERSION2, client_id=settings.client_id, protocol=paho.MQTTv5)
    if settings.use_tls:
        client.tls_set(tls_version=paho.ssl.PROTOCOL_TLS)
    client.username_pw_set(settings.username, settings.password)
    client._on_log = _on_log
    client._on_connect = _on_connect
    client._on_message = _on_message
    client.connect(
        host=settings.address,
        port=int(settings.port),
        keepalive=60,
        clean_start=True)
    return client


def _on_connect(client, userdata, _connect_flags, reason_code, _properties):
    if reason_code == 0:
        logger.info(f"Connected to HiveMQ")
        if userdata['topics']:
            subscriptions = [(topic.strip(), 1) for topic in userdata['topics'].split(',')]
            client.subscribe(subscriptions)
            logger.info(f"Subscribed to topics {userdata['topics']}")
        else:
            logger.info("No topic provided for subscription")
    else:
        logger.error(f"Failed to connect to HiveMQ: {reason_code}")


def _on_message(_client, userdata, msg):
    body = {
        'state': "instant",
        'caption': "MQTT Message",
        'description': msg.payload.decode('utf-8'),
    }
    if userdata['vms_client'].create_generic_event("MQTT Message", msg.payload.decode('utf-8')):
        logger.debug("Event created")
    else:
        logger.warning("Failed to create event")
    logger.debug(json.dumps(body))


def _on_log(_client, _userdata, _level, buf):
    logger.debug(f"MQTT log message: {buf}")


async def mqtt_listener(app: web.Application):
    mqtt_client: Optional[paho.Client] = None

    try:
        host_connector = app['host_connector']
        mqtt_client = _connect_to_hivemq(host_connector.settings.mqtt)
        mqtt_client.user_data_set({
            'topics': host_connector.settings.mqtt.topics,
            'vms_client': VmsHttpClient(host_connector.settings.vms_settings),
        })
        host_connector.set_mqtt_client(mqtt_client)
        mqtt_client.loop_start()
        app['mqtt_client'] = mqtt_client
    except Exception as e:
        app['mqtt_client'] = None
        logger.error(f"Error while initializing MQTT client: {e!r}")

    yield

    if mqtt_client:
        logger.info("Shutting down MQTT client")
        mqtt_client.loop_stop()


async def publish_message(
        client: paho.Client, params: dict[str, str]) -> tuple[str, MqttPublishResult]:
    if not client.is_connected():
        logger.error("MQTT client not connected")
        return "MQTT client not connected", MqttPublishResult.PUBLISHING_ERROR

    if not (topic := params.get('topic', None)):
        return "No topic provided", MqttPublishResult.BAD_PARAMETERS

    if not (payload := params.get('message', None)):
        return "No message body provided", MqttPublishResult.BAD_PARAMETERS

    try:
        qos = int(params.get('qos', 0))
        if qos not in [0, 1, 2]:
            raise ValueError(qos)
    except ValueError as e:
        return f"Invalid QoS value: {str(e)!r}", MqttPublishResult.BAD_PARAMETERS

    try:
        correlation_data = (
            params['correlation_data'].encode('utf-8') if 'correlation_data' in params else None)
    except UnicodeEncodeError as e:
        return f"Invalid correlation data value: {str(e)!r}", MqttPublishResult.BAD_PARAMETERS

    retain = bool(params.get('retain', False))

    try:
        properties = paho.Properties(paho.PacketTypes.PUBLISH)
        if correlation_data is not None:
            properties.CorrelationData = correlation_data
        serialized_payload = json.dumps(payload)
        client.publish(
            topic=topic, payload=serialized_payload, qos=qos, retain=retain, properties=properties)
    except Exception as e:
        logger.error(f"Failed to publish message: {e}")
        return f"Failed to publish message: {str(e)!r}", MqttPublishResult.PUBLISHING_ERROR

    return f"Message {payload!r} is published to topic {topic!r}", MqttPublishResult.SUCCESS
