## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import asyncio
import logging

from aiohttp import web

from integration.host_connector import HostConnector
import integration.detail.mqtt_client as mqtt_client

logger = logging.getLogger(f"mqtt_plugin.{__name__}")

routes = web.RouteTableDef()


@routes.post('/send')
async def send(request):
    if not (client := request.app['mqtt_client']):
        return web.Response(status=500, text=f"MQTT client is not initialized")

    data = await request.json()
    result, status = await mqtt_client.publish_message(client=client, params=data)
    http_status = (200
                   if status == mqtt_client.MqttPublishResult.SUCCESS
                   else 422 if status == mqtt_client.MqttPublishResult.BAD_PARAMETERS
                   else 500)
    return web.Response(status=http_status, text=result)


async def http_server(host_connector: HostConnector):
    app = web.Application()
    app.add_routes(routes)
    app['host_connector'] = host_connector
    app.cleanup_ctx.append(mqtt_client.mqtt_listener)

    runner = web.AppRunner(app)
    await runner.setup()

    http_server_settings = host_connector.settings.http_server
    site = web.TCPSite(
        runner,
        host=http_server_settings.address,
        port=http_server_settings.port)
    await site.start()

    while not host_connector.server_restart_requested:
        await asyncio.sleep(1)

    await runner.cleanup()
