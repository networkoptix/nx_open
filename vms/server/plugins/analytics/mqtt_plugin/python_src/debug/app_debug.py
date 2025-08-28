## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from threading import Thread
import os
import time

import app
import util.app_host_emulator as base_app_host_emulator

RUN_EMULATOR = False
SETTINGS = {
    'http_server_address': '0.0.0.0',
    'http_server_port': '5000',
    'mqtt_address': 'c1d0c485cf51484a9c770f88bff7ab4d.s1.eu.hivemq.cloud',
    'mqtt_client_id': 'test_client_unix',
    'mqtt_password': os.getenv('TEST_MQTT_PASSWORD'),
    'mqtt_port': '8883',
    'mqtt_topic': 'test',
    'mqtt_use_tls': True,
    'mqtt_username': 'test_client_linux',
    'vms_settings_password': os.getenv('TEST_VMS_SERVER_PASSWORD'),
    'vms_settings_server_address': 'https://localhost',
    'vms_settings_server_port': '7001',
    'vms_settings_username': 'admin',
    'misc_log_level': 'DEBUG',
}


def emulate_app_host():
    time.sleep(1)

    while RUN_EMULATOR:
        base_app_host_emulator.call_integration_function(
            command="apply_settings", parameters=SETTINGS
        )
        time.sleep(30)


def run_emulator():
    global RUN_EMULATOR
    RUN_EMULATOR = True
    thread = Thread(target=emulate_app_host)
    thread.start()
    return thread


def stop_emulator(thread: Thread):
    global RUN_EMULATOR
    RUN_EMULATOR = False
    thread.join()


def main():
    emulator_thread = run_emulator()
    app.main()
    stop_emulator(emulator_thread)


if __name__ == '__main__':
    main()
