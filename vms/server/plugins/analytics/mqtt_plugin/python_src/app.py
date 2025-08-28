## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from pathlib import Path
from typing import Optional
import sys
import time

import logging

from integration.host_connector import HostConnector
from integration.settings import get_settings_model
from integration.server import run_server


logger = logging.getLogger('mqtt_plugin')


def _get_default_server_log_dir() -> Optional[Path]:
    if sys.platform == 'linux':
        return Path(__file__).parent.parent.parent.parent / 'var/log'

    if sys.platform.startswith('win'):
        import os
        installation_path = Path(__file__).parent.parent.parent.parent
        vendor = installation_path.parent.name
        application_name = f'{vendor} Media Server'
        return Path(os.getenv('LOCALAPPDATA')) / vendor / application_name / 'log'

    # Unsupported platform
    return None


def setup_logging():
    log_format_string = '%(asctime)s [%(levelname)s] %(message)s'

    logger.setLevel(logging.INFO)
    stderr_handler = logging.StreamHandler(sys.stderr)
    stderr_handler.setFormatter(logging.Formatter(f'[mqtt_plugin] {log_format_string}'))
    logger.addHandler(stderr_handler)

    vms_log_dir = _get_default_server_log_dir()
    if vms_log_dir and vms_log_dir.is_dir():
        file_handler = logging.FileHandler(vms_log_dir / 'mqtt_plugin.log')
        file_handler.setFormatter(logging.Formatter(log_format_string))
        logger.addHandler(file_handler)


def main():
    setup_logging()

    host_connector = HostConnector(get_settings_model())
    host_connector.start()
    host_connector.wait_ready_state()

    while not host_connector.server_shutdown_requested:
        try:
            run_server(host_connector)
        except Exception as e:
            logger.error(f"Error while starting HTTP server: {e!r}. Retrying...")
            time.sleep(1)
        host_connector.notify_server_stopped()

    host_connector.stop()


if __name__ == '__main__':
    main()
