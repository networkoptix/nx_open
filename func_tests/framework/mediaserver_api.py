import json
import logging

from framework.http_api import HttpApi, HttpClient, HttpError

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

    def auth_key(self, method):
        path = ''
        response = self.get('api/getNonce')
        realm, nonce = response['realm'], response['nonce']
        return self.http.auth_key(method, path, realm, nonce)

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

    def request(self, method, path, secure=False, timeout=None, auth=None, **kwargs):
        response = self.http.request(method, path, secure=secure, timeout=timeout, auth=auth, **kwargs)
        if response.is_redirect:
            raise InappropriateRedirect(self._alias, response.request.url, response.next.url)
        data = self._retrieve_data(response)
        self._raise_for_status(response)
        return data

    def credentials_work(self):
        try:
            self.get('ec2/testConnection')
        except Unauthorized:
            return False
        return True
