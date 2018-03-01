"""Proxy object for accessing REST API.

Allows calls to REST API using notation:
    rest_api_object.api.manualCamera.status.GET(arg1=1, arg2=2)
which automatically translated to
    GET /api/manualCamera/status?arg1=1&arg2=2
But for POST method keyword parameters are translated to json request body.
"""

import datetime
import json
import logging

import requests
import requests.exceptions
from requests.auth import HTTPDigestAuth

REST_API_USER = 'admin'
REST_API_PASSWORD = 'admin'
STANDARD_PASSWORDS = [REST_API_PASSWORD, 'qweasd123']  # do not mask these passwords in log files
REST_API_TIMEOUT = datetime.timedelta(seconds=10)
MAX_CONTENT_LEN_TO_LOG = 1000

log = logging.getLogger(__name__)


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


class RestApiError(Exception):
    """Error received from server."""

    def __init__(self, server_name, url, error, error_string):
        super(RestApiError, self).__init__(
            self, 'Server %s at %s REST API request returned error: [%s] %s' % (server_name, url, error, error_string))
        self.error = error
        self.error_string = error_string


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
    def GET(self, raise_exception=True, timeout=None, headers=None, **kw):
        params = {name: _to_get_param(value) for name, value in kw.items()}
        return self._api.request('GET', self._path, raise_exception, timeout=timeout, headers=headers, params=params)

    # noinspection PyPep8Naming
    def POST(self, raise_exception=True, timeout=None, headers=None, json=None, **kw):
        if kw:
            assert not json, 'kw and json arguments are mutually exclusive - only one may be used at a time'
            json = kw
        return self._api.request('POST', self._path, raise_exception, timeout=timeout, headers=headers, json=json)


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

    def __init__(
            self,
            server_name, root_url,
            username=REST_API_USER, password=REST_API_PASSWORD,
            timeout=None,
            ca_cert=None,
            ):
        self.server_name = server_name
        self._root_url = root_url.rstrip('/')
        self.url = self._root_url + '/'
        assert timeout is None or isinstance(timeout, datetime.timedelta), repr(timeout)
        self._default_timeout = timeout or REST_API_TIMEOUT
        self._session = requests.Session()
        if ca_cert is not None:
            self.ca_cert = ca_cert
            log.info("We will trust CA cert: %s.", self.ca_cert)
        self._auth = HTTPDigestAuth(username, password)

    def __enter__(self):
        self._session.__enter__()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._session.__exit__(exc_type, exc_val, exc_tb)

    def __getattr__(self, name):
        return _RestApiProxy(self, '/' + name)

    def __repr__(self):
        return self.__class__.__name__ + str((
            self.server_name,
            self._root_url,
            self._auth.username,
            self._auth.password if self._auth.password in STANDARD_PASSWORDS else '***'))

    @property
    def user(self):
        return self._auth.username

    @property
    def password(self):
        return self._auth.password

    def set_credentials(self, username, password):
        self._auth = HTTPDigestAuth(username, password)

    def get_api_fn(self, method, api_object, api_method):
        object = getattr(self, api_object)  # server.rest_api.ec2
        function = getattr(object, api_method)  # server.rest_api.ec2.getUsers
        return getattr(function, method)  # server.rest_api.ec2.getUsers.GET

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
            raise HttpError(self.server_name, response.request.url, response.status_code, reason, response_data)
        else:
            response.raise_for_status()

    def _retrieve_data(self, response):
        if not response.content:
            log.warning("No response data.")
            return None
        try:
            response_data = response.json()
        except ValueError:
            log.warning("Non-JSON response:\n%r", response.content)
            return response.content

        log.debug("Response data:\n%s", json.dumps(response_data, indent=4))

        if not isinstance(response_data, dict):
            return response_data
        try:
            error_code = int(response_data['error'])
        except KeyError:
            return response_data
        if error_code != 0:
            raise RestApiError(self.server_name, response.request.url, error_code, response_data['errorString'])
        return response_data['reply']

    def request(self, method, path, raise_exception=True, new_connection=False, timeout=None, **kwargs):
        log.debug('%r: %s %s\n%s', self, method, path, json.dumps(kwargs, indent=4))
        try:
            make_request = requests.request if new_connection else self._session.request
            response = make_request(
                method, self._root_url + path,
                auth=self._auth,
                timeout=(timeout or self._default_timeout).total_seconds(),
                verify=str(self.ca_cert), **kwargs)
        except requests.ConnectionError as e:
            if not new_connection:
                log.error("Try new connection after %r.", e)
                return self.request(
                    method, path,
                    raise_exception=raise_exception, new_connection=True,
                    timeout=timeout, **kwargs)
            raise
        if not raise_exception:
            return response.content
        data = self._retrieve_data(response)
        self._raise_for_status(response)
        return data
