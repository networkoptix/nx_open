## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import logging

import requests

from util.integration_settings import IntegrationSettingsSection

logger = logging.getLogger(f"mqtt_plugin.{__name__}")


class VmsHttpClient:
    def __init__(self, settings: IntegrationSettingsSection):
        self._settings = settings
        self._session = requests.Session()

        base_url = f"{settings.server_address}:{settings.server_port}"
        if not (base_url.startswith('http://') or base_url.startswith('https://')):
            base_url = f"https://{base_url}"
        self._base_url = base_url

    @property
    def base_url(self) -> str:
        return self._base_url

    def authenticate(self) -> bool:
        credentials = {'username': self._settings.username, 'password': self._settings.password}
        response = self._session.post(
            f'{self.base_url}/rest/v4/login/sessions', json=credentials, verify=False)
        if response.status_code != 200:
            logger.warning(f"Failed to authenticate: {response.text!r}")
            return False
        logger.debug(f"Authenticated on server")
        token = response.json()['token']
        self._session.headers.update({'Authorization': f'Bearer {token}'})
        return True

    def create_generic_event(self, title: str, message: str) -> bool:
        body = {
            'state': 'instant',
            'caption': title,
            'description': message,
        }
        response = self._session.post(
            f'{self.base_url}/rest/v4/events/generic', json=body, verify=False)
        logger.debug(f"Create event response: {response.status_code} {response.text!r}")

        if response.status_code == 401:
            logger.warning(f"Authentication required: {response.status_code} {response.text!r}")
            if self.authenticate():
                response = self._session.post(
                    f'{self.base_url}/rest/v4/events/generic', json=body, verify=False)

        if response.status_code != 200:
            logger.warning(f"Failed to create event: {response.status_code} {response.text!r}")
            return False

        return True
