'''Proxy object for accessing REST API.

Allows calls to REST API using notation:
    rest_api_object.api.manualCamera.status.GET(arg1=1, arg2=2)
which automatically translated to
    GET /api/manualCamera/status?arg1=1&arg2=2
But for POST method keyword parameters are translated to json request body.
'''

import datetime
import json
import logging
import warnings

import requests
import requests.exceptions
from requests.auth import HTTPDigestAuth
from urllib3.exceptions import InsecureRequestWarning

REST_API_USER = 'admin'
REST_API_PASSWORD = 'admin'
STANDARD_PASSWORDS = [REST_API_PASSWORD, 'qweasd123']  # do not mask these passwords in log files
REST_API_TIMEOUT = datetime.timedelta(seconds=10)
MAX_CONTENT_LEN_TO_LOG = 1000

log = logging.getLogger(__name__)


class HttpError(RuntimeError):

    def __init__(self, server_name, url, status_code, reason, json=None):
         RuntimeError.__init__(self, '[%d] HTTP Error: %r for server %s url: %s' % (status_code, reason, server_name, url))
         self.status_code = status_code
         self.reason = reason
         self.json = json


class ServerRestApiError(RuntimeError):

    def __init__(self, server_name, url, error, error_string):
        RuntimeError.__init__(self, 'Server %s at %s REST API request returned error: [%s] %s' % (server_name, url, error, error_string))
        self.error = error
        self.error_string = error_string


class ServerRestApiProxy(object):

    def __init__(self, server_name, url, user, password, timeout):
        assert isinstance(timeout, datetime.timedelta), repr(timeout)
        self._server_name = server_name
        self._url = url
        self._user = user
        self._password = password
        self._timeout = timeout

    def __getattr__(self, name):
        return ServerRestApiProxy(self._server_name, self._url + '/' + name, self._user, self._password, self._timeout)

    def GET(self, raise_exception=True, timeout=None, headers=None, **kw):
        params = {name: self._get_param_to_str(value) for name, value in kw.items()}
        self._log_request('GET', params)
        return self._make_request(raise_exception, timeout, requests.get, self._url, headers=headers, params=params)

    def POST(self, raise_exception=True, timeout=None, headers=None, json=None, **kw):
        if kw:
            assert not json, 'kw and json arguments are mutually exclusive - only one may be used at a time'
            json = kw
        self._log_request('POST', json)
        return self._make_request(raise_exception, timeout, requests.post, self._url, headers=headers, json=json)

    def _log_request(self, method, data):
        password = self._password
        if password not in STANDARD_PASSWORDS:
            password = '***'  # mask nondefault passwords
        log.debug('%s: %s:%s %s %s %s', self._server_name, self._user, password, method, self._url, json.dumps(data))

    def _get_param_to_str(self, value):
        if type(value) is bool:
            return value and 'true' or 'false'
        return str(value)

    def _make_request(self, raise_exception, timeout, fn, url, *args, **kw):
        timeout_sec = (timeout or self._timeout).total_seconds()
        try:
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', InsecureRequestWarning)
                response = fn(url, auth=HTTPDigestAuth(self._user, self._password),
                              timeout=timeout_sec, verify=False, *args, **kw)
        except requests.exceptions.RequestException as x:
            log.debug('\t--> %s: %s', x.__class__.__name__, x)
            raise
        log.debug('\t--> [%d] %s', response.status_code,
                      response.content if len(response.content) <= MAX_CONTENT_LEN_TO_LOG
                      else response.content[:MAX_CONTENT_LEN_TO_LOG] + '...')
        if raise_exception:
            if 400 <= response.status_code < 600:
                reason = response.reason
                json = None
                if response.content:
                    try:
                        json = response.json()
                    except ValueError:
                        pass
                    else:
                        error_string = json.get('errorString')
                        if error_string:
                            reason = error_string
                raise HttpError(self._server_name, url, response.status_code, reason, json)
            else:
                response.raise_for_status()
        if not response.content:
            return None
        json = response.json()
        if type(json) != type({}) or not raise_exception:
            return json
        error = json.get('error')
        if error is None:
            return json
        if int(error):
            log.debug('%s', json)
            raise ServerRestApiError(self._server_name, self._url, json['error'], json['errorString'])
        else:
            return json['reply']


class RestApiBase(object):

    def __init__(self, server_name, url, user, password, timeout=None):
        assert url.endswith('/'), repr(url)  # http://localhost:7001/
        self.server_name = server_name
        self.url = url
        self.user = user
        self.password = password
        self.timeout = timeout or REST_API_TIMEOUT

    def set_credentials(self, user, password):
        self.user = user
        self.password = password

    def get_api_fn(self, method, api_object, api_method):
        object = getattr(self, api_object)     # server.rest_api.ec2
        function = getattr(object, api_method) # server.rest_api.ec2.getUsers
        return getattr(function, method)       # server.rest_api.ec2.getUsers.GET

    def _make_proxy(self, path):
        return ServerRestApiProxy(self.server_name, self.url + path, self.user, self.password, self.timeout)


class ServerRestApi(RestApiBase):

    @property
    def ec2(self):
        return self._make_proxy('ec2')

    @property
    def api(self):
        return self._make_proxy('api')


class CloudRestApi(RestApiBase):

    @property
    def cdb(self):
        return self._make_proxy('cdb')

    @property
    def api(self):
        return self._make_proxy('api')
