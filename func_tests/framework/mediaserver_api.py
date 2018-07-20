import json
import logging
import time
import timeit
from datetime import datetime
from uuid import UUID

import requests
from pytz import utc

from framework.http_api import HttpApi, HttpClient, HttpError
from framework.utils import RunningTime
from framework.waiting import wait_for_true

DEFAULT_API_USER = 'admin'
INITIAL_API_PASSWORD = 'admin'
DEFAULT_API_PASSWORD = 'qweasd123'
MAX_CONTENT_LEN_TO_LOG = 1000

_logger = logging.getLogger(__name__)


class Unauthorized(HttpError):
    pass


class MediaserverApiError(Exception):
    """Error received from server."""

    def __init__(self, server_name, url, error, error_string):
        super(MediaserverApiError, self).__init__(
            self,
            'Mediaserver {} at {} REST API request returned error: {} {}'.format(
                server_name, url, error, error_string))
        self.error = error
        self.error_string = error_string


class InappropriateRedirect(Exception):
    def __init__(self, server_name, url, location):
        message = 'Mediaserver {} redirected {} to {}'.format(server_name, url, location)
        super(InappropriateRedirect, self).__init__(self, message)


class GenericMediaserverApi(HttpApi):
    @classmethod
    def new(cls, alias, hostname, port, username='admin', password=INITIAL_API_PASSWORD, ca_cert=None):
        return cls(alias, HttpClient(hostname, port, username, password, ca_cert=ca_cert))

    def _raise_for_status(self, response):
        if 400 <= response.status_code < 600:
            reason = response.reason
            response_data = None
            if response.content:
                try:
                    response_data = response.json()
                except ValueError:
                    pass
                else:
                    error_string = response_data.get('errorString')
                    if error_string:
                        reason = error_string
            if response.status_code == 401:
                raise Unauthorized(self._alias, response.request.url, response.status_code, reason, response_data)
            raise HttpError(self._alias, response.request.url, response.status_code, reason, response_data)
        else:
            response.raise_for_status()

    def _retrieve_data(self, response):
        if not response.content:
            _logger.warning("Empty response.")
            return None
        try:
            response_data = response.json()
        except ValueError:
            _logger.warning("Non-JSON response:\n%s", response.content)
            return response.content
        _logger.debug("JSON response:\n%s", json.dumps(response_data, indent=4))
        if not isinstance(response_data, dict):
            return response_data
        try:
            error_code = int(response_data['error'])
        except KeyError:
            return response_data
        if error_code != 0:
            raise MediaserverApiError(self._alias, response.request.url, error_code, response_data['errorString'])
        return response_data['reply']

    def request(self, method, path, secure=False, timeout=None, **kwargs):
        response = self.http.request(method, path, secure=secure, timeout=timeout, **kwargs)
        if response.is_redirect:
            raise InappropriateRedirect(self._alias, response.request.url, response.next.url)
        data = self._retrieve_data(response)
        self._raise_for_status(response)
        return data


class MediaserverApi(object):
    def __init__(self, generic_api):  # type: (GenericMediaserverApi) -> None
        self.generic = generic_api

    def auth_key(self, method):
        path = ''
        response = self.generic.get('api/getNonce')
        realm, nonce = response['realm'], response['nonce']
        return self.generic.http.auth_key(method, path, realm, nonce)

    def is_online(self):
        try:
            self.generic.get('/api/ping')
        except requests.RequestException:
            return False
        else:
            return True

    def credentials_work(self):
        try:
            self.generic.get('ec2/testConnection')
        except Unauthorized:
            return False
        return True

    def get_server_id(self):
        return self.generic.get('/ec2/testConnection')['ecsGuid']

    def get_system_settings(self):
        settings = self.generic.get('/api/systemSettings')['settings']
        return settings

    def get_local_system_id(self):
        response = requests.get(self.generic.http.url('api/ping'))
        return UUID(response.json()['reply']['localSystemId'])

    def set_local_system_id(self, new_id):
        self.generic.get('/api/configure', params={'localSystemId': str(new_id)})

    def get_cloud_system_id(self):
        return self.get_system_settings()['cloudSystemID']

    def get_time(self):
        started_at = datetime.now(utc)
        time_response = self.generic.get('/api/gettime')
        received = datetime.fromtimestamp(float(time_response['utcTime']) / 1000., utc)
        return RunningTime(received, datetime.now(utc) - started_at)

    def is_primary_time_server(self):
        response = self.generic.get('api/systemSettings')
        return response['settings']['primaryTimeServer'] == self.get_server_id()

    def restart_via_api(self, timeout_sec=10):
        old_runtime_id = self.generic.get('api/moduleInformation')['runtimeId']
        _logger.info("Runtime id before restart: %s", old_runtime_id)
        started_at = timeit.default_timer()
        self.generic.get('api/restart')
        failed_connections = 0
        while True:
            try:
                response = self.generic.get('api/moduleInformation')
            except requests.ConnectionError as e:
                if timeit.default_timer() - started_at > timeout_sec:
                    assert False, "Mediaserver hasn't started, caught %r, timed out." % e
                _logger.debug("Expected failed connection: %r", e)
                failed_connections += 1
                time.sleep(timeout_sec)
                continue
            new_runtime_id = response['runtimeId']
            if new_runtime_id == old_runtime_id:
                if failed_connections > 0:
                    assert False, "Runtime id remains same after failed connections."
                if timeit.default_timer() - started_at > timeout_sec:
                    assert False, "Mediaserver hasn't stopped, timed out."
                _logger.warning("Mediaserver hasn't stopped yet, delay is acceptable.")
                time.sleep(timeout_sec)
                continue
            _logger.info("Mediaserver restarted successfully, new runtime id is %s", new_runtime_id)
            break

    def factory_reset(self):
        old_runtime_id = self.generic.get('api/moduleInformation')['runtimeId']
        self.generic.post('api/restoreState', {})

        def _mediaserver_has_restarted():
            try:
                current_runtime_id = self.generic.get('api/moduleInformation')['runtimeId']
            except requests.RequestException:
                return False
            return current_runtime_id != old_runtime_id

        wait_for_true(_mediaserver_has_restarted)

    def get_updates_state(self):
        response = self.generic.get('api/updates2/status')
        status = response['state']
        return status

    def add_camera(self, camera):
        assert not camera.id, 'Already added to a server with id %r' % camera.id
        params = camera.get_info(parent_id=self.get_server_id())
        result = self.generic.post('ec2/saveCamera', dict(**params))
        camera.id = result['id']
        return camera.id
