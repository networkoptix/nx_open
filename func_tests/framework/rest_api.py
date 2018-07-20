import base64
import hashlib
import json
import logging
from abc import ABCMeta, abstractmethod
from pprint import pformat

import requests
import requests.exceptions
from requests.auth import HTTPDigestAuth

DEFAULT_API_USER = 'admin'
INITIAL_API_PASSWORD = 'admin'
DEFAULT_API_PASSWORD = 'qweasd123'
STANDARD_PASSWORDS = [DEFAULT_API_PASSWORD, INITIAL_API_PASSWORD]  # do not mask these passwords in log files
REST_API_TIMEOUT_SEC = 20
MAX_CONTENT_LEN_TO_LOG = 1000

_logger = logging.getLogger(__name__)


def _to_get_param(python_value):
    if isinstance(python_value, bool):
        return 'true' if python_value else 'false'
    if isinstance(python_value, (int, float, str, bytes, unicode)):
        return str(python_value)
    assert False, "Cannot use %r of type %s as GET parameter." % (python_value, type(python_value).__name__)


class HttpError(Exception):
    """Error on HTTP or connection."""

    def __init__(self, server_name, url, status_code, reason, json=None):
        super(HttpError, self).__init__(
            self, '[%d] HTTP Error: %r for server %s url: %s' % (status_code, reason, server_name, url))
        self.status_code = status_code
        self.reason = reason
        self.json = json


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


class HttpClient(object):
    def __init__(self, hostname, port, username, password, ca_cert=None):
        self._port = port
        self._hostname = hostname
        self.ca_cert = ca_cert
        if self.ca_cert is not None:
            _logger.info("Trust CA cert: %s.", self.ca_cert)
        self._auth = HTTPDigestAuth(username, password)
        self.user = username  # Only for interface.
        self.password = password  # Only for interface.

    def __repr__(self):
        password_display = self._auth.password if self._auth.password in STANDARD_PASSWORDS else '***'
        return "<HttpClient {} {}:{}>".format(self.url(''), self._auth.username, password_display)

    def set_credentials(self, username, password):
        """If credentials were changed"""
        self.user = username
        self.password = password
        self._auth = HTTPDigestAuth(username, password)

    def auth_key(self, method, path, realm, nonce):
        # `requests.auth.HTTPDigestAuth.build_digest_header` does the same but it substitutes empty path with '/'.
        # No straightforward way of getting key has been found.
        # This method is used only for specific tests, so there is no need to save one HTTP request.
        ha1 = hashlib.md5(':'.join([self.user.lower(), realm, self.password]).encode()).hexdigest()
        ha2 = hashlib.md5(':'.join([method, path]).encode()).hexdigest()  # Empty path.
        digest = hashlib.md5(':'.join([ha1, nonce, ha2]).encode()).hexdigest()
        key = base64.b64encode(':'.join([self.user.lower(), nonce, digest]))
        return key

    def url(self, path, secure=False):
        return '{}://{}:{}/{}'.format('https' if secure else 'http', self._hostname, self._port, path.lstrip('/'))

    def request(self, method, path, secure=False, timeout=None, auth=None, **kwargs):
        url = self.url(path, secure=secure)
        response = requests.request(
            method, url,
            auth=auth or self._auth,
            verify=str(self.ca_cert),
            allow_redirects=False,
            timeout=timeout or REST_API_TIMEOUT_SEC,
            **kwargs)
        return response


class HttpApi(object):
    __metaclass__ = ABCMeta

    def __init__(self, alias, http_client):  # type: (str, HttpClient) -> None
        self._alias = alias
        self.http = http_client

    @abstractmethod
    def request(self, method, path, secure=False, timeout=None, auth=None, **kwargs):
        return {}

    def get(self, path, params=None, **kwargs):
        _logger.debug('GET params:\n%s', pformat(params, indent=4))
        assert 'data' not in kwargs
        assert 'json' not in kwargs
        return self.request('GET', path, params=params, **kwargs)

    def post(self, path, data, **kwargs):
        _logger.debug('JSON payload:\n%s', json.dumps(data, indent=4))
        return self.request('POST', path, json=data, **kwargs)


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


class GenericCloudApi(HttpApi):
    def request(self, method, path, secure=False, timeout=None, auth=None, **kwargs):
        response = self.http.request(method, path, secure=secure, timeout=timeout, auth=auth, **kwargs)
        response.raise_for_status()
        data = response.json()
        return data
