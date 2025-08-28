## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import asyncio
import logging

from integration.detail.http_server import http_server
from integration.host_connector import HostConnector

logger = logging.getLogger(f"mqtt_plugin.{__name__}")


def set_log_level(log_level: str):
    try:
        logger = logging.getLogger('mqtt_plugin')
        logger.setLevel(log_level)
        for handler in logger.handlers:
            handler.setLevel(log_level)

        logger.info(f"Log level changed to {log_level}")
    except Exception as e:
        logger.error(f"Error changing log level: {str(e)}")


# This method MUST present in the "integration/server.py" file. If the plugin does not need any
# server functionality, the method should be an endless loop, aborting when
# host_connector.server_shutdown_requested is True.
def run_server(host_connector: HostConnector):
    set_log_level(host_connector.settings.misc.log_level)
    asyncio.run(http_server(host_connector))
