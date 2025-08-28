## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from threading import Thread
import logging
import time

from util.integration_settings import IntegrationSettings
try:
    import nx_integration_interface
except ImportError:
    import util.nx_integration_interface_dummy as nx_integration_interface

logger = logging.getLogger(f"mqtt_plugin.{__name__}")


class BaseHostConnector:
    def __init__(self, settings_model: IntegrationSettings):
        self.settings = settings_model
        self._server_shutdown_requested = False
        self._server_restart_requested = False
        self._is_connector_active = True
        self._thread = Thread(target=self.run)

    def run(self):
        while self._is_connector_active:
            command: nx_integration_interface.Command = nx_integration_interface.get_host_request()
            logger.debug(f"Request from host received: {command.command}({command.parameters!r})")
            if command.request_id == nx_integration_interface.EMPTY_REQUEST_ID:
                logger.debug("Empty request id, skipping")
                continue
            if not (function_to_call := getattr(self, command.command, None)):
                logger.warning(f"Unknown function name in host request: {command.command!r}")
                continue
            result = function_to_call(command.parameters)
            if command.request_id != nx_integration_interface.IRRELEVANT_REQUEST_ID:
                nx_integration_interface.return_app_reply(command.request_id, str(result))

    def start(self):
        self._thread.start()

    def stop(self):
        self._is_connector_active = False
        self._thread.join()

    def wait_ready_state(self):
        while not self.settings:
            time.sleep(1)

    @property
    def server_shutdown_requested(self):
        return self._server_shutdown_requested

    @property
    def server_restart_requested(self):
        return self._server_restart_requested or self._server_shutdown_requested

    def notify_server_stopped(self):
        self._server_restart_requested = False

    def apply_settings(self, settings: dict[str, str]):
        is_first_time = not self.settings
        self.settings.load_settings(settings)
        if not is_first_time:
            self.request_server_restart()

    def settings_model(self, _) -> str:
        return self.settings.generate_manifest()

    def object_actions_model(self, _) -> str:
        return "[]"

    def execute_object_action(self, params: dict[str, str]) -> str:
        return f"Action {params.get('_action_id', 'unknown')!r} not implemented"

    def request_server_restart(self):
        self._server_restart_requested = True

    def request_server_shutdown(self):
        self._server_shutdown_requested = True
