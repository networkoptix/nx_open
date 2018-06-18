"""Proxy object for accessing REST API.

Allows calls to REST API using notation:
    api_object.api.manualCamera.status.GET(arg1=1, arg2=2)
which automatically translated to
    GET /api/manualCamera/status?arg1=1&arg2=2
But for POST method keyword parameters are translated to json request body.
"""
import base64
import csv
import datetime
import hashlib
import json
import json as json_module
import logging
from pprint import pformat

import requests
import requests.exceptions
from requests.auth import HTTPDigestAuth

DEFAULT_API_USER = 'admin'
DEFAULT_API_PASSWORD = 'admin'
STANDARD_PASSWORDS = [DEFAULT_API_PASSWORD, 'qweasd123']  # do not mask these passwords in log files
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


class RestApiError(Exception):
    """Error received from server."""

    def __init__(self, server_name, url, error, error_string):
        super(RestApiError, self).__init__(
            self,
            'Mediaserver {} at {} REST API request returned error: {} {}'.format(
                server_name, url, error, error_string))
        self.error = error
        self.error_string = error_string


class InappropriateRedirect(Exception):
    def __init__(self, server_name, url, location):
        message = 'Mediaserver {} redirected {} to {}'.format(server_name, url, location)
        super(InappropriateRedirect, self).__init__(self, message)


class _RestApiProxy(object):
    """
    >>> api = RestApi('HTTP Request & Response Service', 'http://httpbin.org', '', '')
    >>> directory = _RestApiProxy(api, '/')
    >>> directory.get.GET(wise='choice')  # doctest: +ELLIPSIS
    {...wise...choice...}
    >>> directory.post.POST(wise='choice')  # doctest: +ELLIPSIS
    {...wise...choice...}
    """

    def __init__(self, api, url):
        self._path = url
        self._api = api

    def __getattr__(self, name):
        return _RestApiProxy(self._api, self._path + '/' + name)

    # noinspection PyPep8Naming
    def GET(self, timeout=None, headers=None, **kw):
        params = {name: _to_get_param(value) for name, value in kw.items()}
        _logger.debug('GET params:\n%s', pformat(params, indent=4))
        return self._api.request('GET', self._path, timeout=timeout, headers=headers, params=params)

    # noinspection PyPep8Naming
    def POST(self, timeout=None, headers=None, json=None, **kw):
        if kw:
            assert not json, 'kw and json arguments are mutually exclusive - only one may be used at a time'
            json = kw
        _logger.debug('JSON payload:\n%s', json_module.dumps(json, indent=4))
        return self._api.request('POST', self._path, timeout=timeout, headers=headers, json=json)


class RestApi(object):
    """Mimic requests.Session.request.

    >>> api = RestApi('HTTP Request & Response Service', 'http://httpbin.org', 'u', 'p')
    >>> api.request('GET', '/get', params={'pure': 'juice'})  # doctest: +ELLIPSIS
    {...pure...juice...}
    >>> api.request('GET', '/digest-auth/md5/u/p')  # doctest: +ELLIPSIS
    {...'authenticated': True...}
    >>> api.request('GET', '/digest-auth/md5/u/pp')  # doctest: +ELLIPSIS
    Traceback (most recent call last):
        ...
    HttpError...401...
    """

    def __init__(self, alias, hostname, port, username='admin', password='admin', ca_cert=None):
        self._port = port
        self._hostname = hostname
        self._alias = alias
        self.ca_cert = ca_cert
        if self.ca_cert is not None:
            _logger.info("Trust CA cert: %s.", self.ca_cert)
        self._auth = HTTPDigestAuth(username, password)
        self.user = username  # Only for interface.
        self.password = password  # Only for interface.

    def __getattr__(self, name):
        return _RestApiProxy(self, '/' + name)

    def __repr__(self):
        password_display = self._auth.password if self._auth.password in STANDARD_PASSWORDS else '***'
        return "<RestApi {} {} {}:{}>".format(self._alias, self.url(''), self._auth.username, password_display)

    def with_credentials(self, username, password):
        """If credentials were changed"""
        return self.__class__(
            self._alias, self._hostname, self._port,
            username=username, password=password,
            ca_cert=self.ca_cert)

    def with_hostname_and_port(self, hostname, port):
        """To access same API with changed networking"""
        return self.__class__(
            self._alias, hostname, port,
            username=self.user, password=self.password,
            ca_cert=self.ca_cert)

    def auth_key(self, method):
        path = ''
        header = self._auth.build_digest_header(method, path)
        if header is None:  # First time requested.
            response = self.get('api/getNonce')
            realm, nonce = response['realm'], response['nonce']
        else:
            key, value = header.split(' ', 1)
            assert key.lower() == 'digest'
            info = dict(csv.reader(value.split(', '), delimiter='=', doublequote=False))
            realm, nonce = info['realm'], info['nonce']
        # requests.auth.HTTPDigestAuth.build_digest_header does the same but it substitutes empty path with '/'.
        ha1 = hashlib.md5(':'.join([self.user.lower(), realm, self.password]).encode()).hexdigest()
        ha2 = hashlib.md5(':'.join([method, path]).encode()).hexdigest()  # Empty path.
        digest = hashlib.md5(':'.join([ha1, nonce, ha2]).encode()).hexdigest()
        key = base64.b64encode(':'.join([self.user.lower(), nonce, digest]))
        return key

    def get_api_fn(self, method, api_object, api_method):
        object = getattr(self, api_object)  # server.api.ec2
        function = getattr(object, api_method)  # server.api.ec2.getUsers
        return getattr(function, method)  # server.api.ec2.getUsers.GET

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
            raise RestApiError(self._alias, response.request.url, error_code, response_data['errorString'])
        return response_data['reply']

    def url(self, path, secure=False):
        return '{}://{}:{}/{}'.format('https' if secure else 'http', self._hostname, self._port, path.lstrip('/'))

    def get(self, path, params=None, **kwargs):
        _logger.debug('GET params:\n%s', pformat(params, indent=4))
        assert 'data' not in kwargs
        assert 'json' not in kwargs
        return self.request('GET', path, params=params, **kwargs)

    def post(self, path, data, **kwargs):
        _logger.debug('JSON payload:\n%s', json.dumps(data, indent=4))
        return self.request('POST', path, json=data, **kwargs)

    def request(self, method, path, secure=False, timeout=None, **kwargs):
        url = self.url(path, secure=secure)
        response = requests.request(
            method, url, auth=self._auth, verify=str(self.ca_cert),
            allow_redirects=False,
            timeout=timeout or REST_API_TIMEOUT_SEC,
            **kwargs)
        if response.is_redirect:
            raise InappropriateRedirect(self._alias, url, response.next.url)
        data = self._retrieve_data(response)
        self._raise_for_status(response)
        return data

    def credentials_work(self):
        try:
            self.get('ec2/testConnection')
        except Unauthorized:
            return False
        return True
